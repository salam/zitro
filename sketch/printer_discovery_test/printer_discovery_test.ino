/*
 * Bonjour/mDNS Printer Discovery Test
 *
 * This sketch discovers Bonjour-enabled printers on the network
 * and displays their details for research purposes.
 *
 * Hardware: ESP32-based boards (M5StickC Plus 2)
 *
 * Test Results:
 * - Note which printers are discovered
 * - Record IP addresses, ports, and service names
 * - Check printer attributes/capabilities
 */

#include <WiFi.h>
#include <ESPmDNS.h>

// Replace with your WiFi credentials
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

struct PrinterInfo {
  String name;
  String hostname;
  IPAddress ip;
  uint16_t port;
  String txt_rp;  // Resource path from TXT record
  String txt_ty;  // Printer type/model
  String txt_pdl; // Supported page description languages
};

PrinterInfo discoveredPrinters[10];
int printerCount = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n========================================");
  Serial.println("Bonjour Printer Discovery Test");
  Serial.println("========================================\n");

  // Connect to WiFi
  Serial.printf("Connecting to WiFi: %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n\nERROR: Failed to connect to WiFi!");
    Serial.println("Please check your credentials and try again.");
    return;
  }

  Serial.println("\n\nWiFi Connected!");
  Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("Subnet Mask: %s\n", WiFi.subnetMask().toString().c_str());
  Serial.printf("Gateway: %s\n\n", WiFi.gatewayIP().toString().c_str());

  // Initialize mDNS
  if (!MDNS.begin("printer-test")) {
    Serial.println("ERROR: Failed to start mDNS!");
    return;
  }
  Serial.println("mDNS responder started\n");

  // Discover printers
  discoverPrinters();

  // Display results
  displayResults();
}

void loop() {
  // Re-scan every 30 seconds
  delay(30000);
  Serial.println("\n\n========================================");
  Serial.println("Re-scanning for printers...");
  Serial.println("========================================\n");
  printerCount = 0;
  discoverPrinters();
  displayResults();
}

void discoverPrinters() {
  Serial.println("Searching for printers via mDNS...\n");

  // Try multiple service types that printers might advertise
  const char* serviceTypes[] = {
    "ipp",      // Internet Printing Protocol
    "ipps",     // IPP over TLS
    "printer",  // Generic printer
    "pdl-datastream", // Page Description Language
    "http"      // Some printers advertise as HTTP
  };

  for (int serviceIdx = 0; serviceIdx < 5; serviceIdx++) {
    Serial.printf("Querying service: _%s._tcp...", serviceTypes[serviceIdx]);
    Serial.flush();  // Force output immediately

    unsigned long startTime = millis();
    int n = MDNS.queryService(serviceTypes[serviceIdx], "tcp");
    unsigned long elapsed = millis() - startTime;

    Serial.printf(" (took %lu ms)\n", elapsed);

    if (n == 0) {
      Serial.println("  No services found\n");
      continue;
    }

    Serial.printf("  Found %d service(s)\n\n", n);

    for (int i = 0; i < n && printerCount < 10; i++) {
      PrinterInfo printer;
      printer.name = MDNS.hostname(i);
      printer.hostname = MDNS.hostname(i);
      printer.ip = MDNS.address(i);  // ESP32 uses address() not IP()
      printer.port = MDNS.port(i);

      // Try to get TXT records
      // Note: ESPmDNS library support for TXT records varies by version
      // This is a placeholder - actual implementation may differ

      Serial.printf("  [%d] %s\n", i + 1, printer.hostname.c_str());
      Serial.printf("      IP: %s\n", printer.ip.toString().c_str());
      Serial.printf("      Port: %d\n", printer.port);
      Serial.printf("      Service: _%s._tcp\n", serviceTypes[serviceIdx]);

      // Check if this printer is already in our list (avoid duplicates)
      bool isDuplicate = false;
      for (int j = 0; j < printerCount; j++) {
        if (discoveredPrinters[j].ip == printer.ip) {
          isDuplicate = true;
          break;
        }
      }

      if (!isDuplicate) {
        discoveredPrinters[printerCount++] = printer;
      }

      Serial.println();
    }
  }
}

void displayResults() {
  Serial.println("\n========================================");
  Serial.println("DISCOVERY SUMMARY");
  Serial.println("========================================\n");

  if (printerCount == 0) {
    Serial.println("No printers discovered.");
    Serial.println("\nTroubleshooting:");
    Serial.println("1. Ensure printer is on and connected to same network");
    Serial.println("2. Check if printer supports Bonjour/mDNS");
    Serial.println("3. Try accessing printer's web interface manually");
    Serial.println("4. Some printers may use non-standard service names");
    return;
  }

  Serial.printf("Total unique printers discovered: %d\n\n", printerCount);

  for (int i = 0; i < printerCount; i++) {
    Serial.println("----------------------------------------");
    Serial.printf("Printer #%d\n", i + 1);
    Serial.println("----------------------------------------");
    Serial.printf("Name:     %s\n", discoveredPrinters[i].name.c_str());
    Serial.printf("Hostname: %s\n", discoveredPrinters[i].hostname.c_str());
    Serial.printf("IP:       %s\n", discoveredPrinters[i].ip.toString().c_str());
    Serial.printf("Port:     %d\n", discoveredPrinters[i].port);
    Serial.println();

    Serial.println("Next Steps for this printer:");
    Serial.printf("1. Try accessing: http://%s:%d/\n",
                  discoveredPrinters[i].ip.toString().c_str(),
                  discoveredPrinters[i].port);
    Serial.printf("2. IPP endpoint likely: ipp://%s:%d/ipp/print\n",
                  discoveredPrinters[i].ip.toString().c_str(),
                  discoveredPrinters[i].port);
    Serial.println("3. Run the Python IPP test client against this IP");
    Serial.println();
  }

  Serial.println("========================================");
  Serial.println("Memory Status:");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("========================================\n");
}
