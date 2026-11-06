# Zitro Relay: subscription key service (design)

Status: proposal, nothing deployed. The firmware already has a `zitro` provider that talks to
`https://relay.zitro.spot/v1/...` (placeholder host), so devices are ready once this service exists.

## 0. Decisions needed first

1. **"We can't read the requests" and "we swap the API key" pull against each other.** To replace the
   `Authorization` header, the relay must end the device's TLS connection, so the request is in plain text in
   the relay's memory for a moment. There are two honest ways to deal with that:
   - **A. Blind relay in an attested enclave (recommended).** The relay runs inside a confidential-computing
     enclave (AWS Nitro Enclaves or GCP Confidential Space) from a public, reproducible build. Devices only
     trust a TLS key that is bound to that exact build. Nobody at Zitro, cloud admins included, can attach a
     debugger, read memory, or change the code without the device noticing. Request and response bodies are
     streamed through and never buffered to disk or logged. Without the enclave this weakens to "we don't
     store it, by policy", and we must not market that as "can't read".
   - **B. Key broker only (truly content-blind).** The relay never sees content. It hands the device a
     short-lived, spend-capped provider key, and the device calls OpenAI or Anthropic directly. The downsides:
     a real provider credential sits on the device, metering depends on the providers' usage reports (delayed
     by minutes to hours), and Anthropic keys can't be created by API (see 4.3).

   This document details A. B is the fallback if enclave costs or complexity are too high.
2. **Fair-use thresholds, as I read your message.** When a subscriber's provider costs for the month reach
   **90 % of their subscription fee**, requests are **throttled**. At **100 %** they are **paused** until the
   next billing period (or until a top-up). Please confirm.
3. **Brand conflict.** The Zitro brief promises "no account". A Stripe subscription needs at least an email
   and a payment method. Proposal: the account exists only for billing, isn't needed to use a device with your
   own keys, and is never linked to what is asked.
4. **Pricing promise conflict.** The website (July release) says "$25, once, no subscription after".
   A monthly plan has to be positioned as optional (bring your own key stays free), or that copy changes.
5. **Which upstream models?** The relay picks the model (`zitro-default`). Proposal: Claude Opus 5.5 for
   answers (low effort), OpenAI for speech, read-aloud and images.

## 1. Goals and threat model

| Goal | How |
| --- | --- |
| Never store or read questions and answers | Streaming proxy in an attested enclave, no body logging, no disk |
| Subscribers never see a provider key | Device holds only a Zitro credential; key translation happens inside the enclave |
| Provider keys roll monthly with no manual work | Automated rotation (OpenAI) or no static key at all (Anthropic via federation) |
| Fair use enforced in near real time | Usage metered inside the enclave from response `usage` fields |
| Tamper-proof | Signed firmware, per-device secrets, attestation-pinned TLS, signed Stripe webhooks, append-only ledger |
| Robust | Stateless relays in 2+ regions, fail closed on auth, fail open only on metering lag (bounded) |

Threats in scope: stolen or cloned device keys, a subscriber trying to extract the upstream key, a
malicious insider or cloud admin, forged payment webhooks, replay of old tokens, and abuse/resale of
subscriptions. Out of scope: a compromised provider (OpenAI or Anthropic).

## 2. Architecture

```
 Zitro device                         relay.zitro.spot (enclave, stateless)                Providers
 ┌──────────────┐  TLS (pinned to   ┌──────────────────────────────────────┐   TLS   ┌─────────────┐
 │ device secret│  attested key)    │ 1. verify access token (Ed25519)     │────────▶│ Anthropic   │
 │ access token ├──────────────────▶│ 2. check entitlement (quota state)   │         │ OpenAI      │
 └──────┬───────┘                   │ 3. swap auth header -> upstream cred │◀────────│             │
        │ token refresh             │ 4. stream body both ways, untouched  │         └─────────────┘
        ▼                           │ 5. read usage numbers -> meter event │
 ┌──────────────┐                   └───────────────┬──────────────────────┘
 │ auth.zitro   │ issues 1 h tokens                  │ meter events (no content)
 │ (no content) │◀── entitlements ──┐                ▼
 └──────────────┘                   │       ┌──────────────────┐     ┌──────────┐
                                    └───────│ billing service  │◀────│ Stripe   │ signed webhooks
                                            │ ledger + quotas  │     └──────────┘
                                            └──────────────────┘
```

Components:
- **Relay** (enclave, stateless, horizontally scaled). The only component that sees content. Exposes an
  OpenAI-compatible API (`/v1/chat/completions`, `/v1/audio/transcriptions`, `/v1/audio/speech`,
  `/v1/images/generations`), so the firmware needs a single code path.
- **Auth service.** Issues short-lived access tokens to devices. Never sees content.
- **Billing service.** Stripe integration, entitlement table, usage ledger, quota state machine.
- **Secrets.** Upstream credentials live in a KMS whose key policy only releases them to an enclave with the
  expected measurement (PCR0/image digest). The operator never sees them in plain text.

## 3. Device credentials (the "Zitro key")

- What a subscriber types into the setup page (`zk_...`) is a **one-time activation code**, not a
  long-lived API key. On first use, the device generates an Ed25519 key pair and registers the public key
  with the auth service, bound to the subscription.
- The device then proves possession of its private key to get **access tokens valid for 1 hour**, signed
  by the auth service. The relay verifies them offline (public key baked in) and checks a revocation list
  that is pushed every minute.
- **Monthly key rolling is automatic and invisible.** The device re-keys itself on a schedule, with an
  overlap window so devices that were offline for weeks still recover with their old key once, then rotate.
- Hardening on the stick: ESP32 secure boot v2 and flash encryption, so the private key can't be read out
  of flash. Rate limits per device and per subscription stop a leaked token from being resold.
- Firmware note: the current build sends the `zk_...` value directly as a bearer token. Before launch, the
  activation and token exchange above replace that, along with TLS pinning (today all connections use
  `setInsecure()`).

## 4. Key translation and upstream credentials

### 4.1 Translation
Inside the enclave: verify the token, look up the subscriber's **upstream pool** (a group of subscribers
sharing an upstream credential, for blast-radius control), remove the device's `Authorization`, add the
upstream credential, and forward. The mapping lives in enclave memory only.

### 4.2 OpenAI
Use the OpenAI Admin API to create a **project service account per pool**. Its key is returned once and
goes straight into KMS, encrypted to the enclave. A monthly job creates the new key, tells relays to start
using it, waits 24 h, then deletes the old one. Each project gets an OpenAI budget limit as a backstop.
*Verify before building: current service-account key creation and budget endpoints.*

### 4.3 Anthropic
The Anthropic Admin API can **list and disable** API keys but **cannot create** them. So instead of rolling
static keys, the relay uses **Workload Identity Federation**: the enclave presents its cloud identity token,
Anthropic exchanges it for a short-lived OAuth token, and the SDK refreshes it automatically. There is no
long-lived Anthropic secret to leak or roll. The one-time federation rule is set up by a human in the Claude
Console. Use one workspace per pool, with a workspace spend limit as a backstop.

### 4.4 Infomaniak
For a Swiss-only data path: Infomaniak AI Tools tokens are created in the Manager. Rotation is a monthly
runbook unless their API allows creating tokens. Treat it as an optional pool.

## 5. Metering and fair use

- **What the enclave reads:** only the numbers it needs. That is `usage` (input/output/cached tokens) from
  chat responses, the audio length from the request's WAV header, the character count for read-aloud, and
  the image count and size. It emits `{subscription_id, model, units, cost_micro_chf, ts}` with no content.
- **Cost table:** per-model prices, versioned in the billing service.
- **Quota state machine, per billing period:**

| Cost so far vs. monthly fee | State | Relay behaviour |
| --- | --- | --- |
| < 90 % | `normal` | Pass through |
| 90-100 % | `throttled` | Max 1 request per 20 s, cheaper model tier, shorter `max_tokens` |
| ≥ 100 % | `paused` | `402` with a friendly message; the device shows "Zitro quota reached" |

- **Enforcement point:** relays keep a local counter per subscription, synced every few seconds through the
  billing service (Redis or a durable counter). The maximum overshoot is bounded by the sync interval times
  the per-request cap.
- Throttling and pausing must be visible: the device shows the message (already in the firmware for
  `402`/`429`), and an email goes out at 80 %.

## 6. Payments (Stripe)

- Stripe Checkout for subscription start and Stripe Billing for monthly invoices. The Customer Portal
  handles cancellations and card changes.
- **Webhooks** (`checkout.session.completed`, `invoice.paid`, `invoice.payment_failed`,
  `customer.subscription.deleted`): signature verified with the endpoint secret, processed idempotently by
  `event.id`, and written to an append-only ledger. Entitlement changes come *only* from verified events,
  never from client calls.
- Grace period: on `invoice.payment_failed`, keep `normal` for 3 days, then `paused`.
- Stripe holds the card data (PCI SAQ-A). Zitro stores the Stripe customer ID, an email, and device public
  keys, and nothing about usage content.

## 7. Safety, robustness, tamper-resistance checklist

- Reproducible relay builds; publish the enclave measurement and source so anyone can verify the "can't
  read" claim.
- The device pins the attestation-bound relay key, with a backup key for rotation.
- No request logging at all in the relay. Operational logs carry only status codes, latency, and a random
  request ID. This is enforced in code review and with a CI check that bans body logging.
- Fail closed: if the token is invalid, entitlement is unknown for longer than 5 minutes, or the KMS is
  unavailable, reject the request.
- Two regions, health-checked, behind anycast. The relay is stateless, so a lost instance loses nothing.
- Secrets never in environment variables or images. They come from KMS and are released to the attested
  enclave only.
- Abuse controls: per-device and per-subscription rate limits, and anomaly alerts when one subscription
  shows many devices or IPs.
- Quarterly penetration test; bug bounty after launch.

## 8. Phased plan

1. **MVP (no enclave):** token service, relay as a plain container, Stripe, metering, quotas. Market it
   only as "not stored".
2. **Enclave:** move the relay into Nitro Enclaves or Confidential Space, attestation-pinned TLS, publish
   the measurement. Only then claim "cannot read".
3. **Device hardening:** secure boot and flash encryption in production firmware, activation flow, and
   pinned TLS.
