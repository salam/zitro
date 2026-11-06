/*
 * Quick Printer Test - Print Test Image
 *
 * This sketch:
 * 1. Connects to WiFi
 * 2. Discovers the first printer via mDNS
 * 3. Queries printer capabilities
 * 4. Prints a test image (embedded in code)
 *
 * No user interaction needed - just upload and watch Serial Monitor!
 *
 * Usage:
 * 1. Update WiFi credentials below
 * 2. Upload sketch
 * 3. Watch Serial Monitor at 115200 baud
 *
 * No SPIFFS upload needed - image is embedded in code!
 */

#include <WiFi.h>
#include <ESPmDNS.h>
#include "test_image_data.h"  // Embedded test image

// ============================================================
// CONFIGURATION - Update these!
// ============================================================
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";  // Update this!

// ============================================================
// Printer Info Structure
// ============================================================
struct PrinterInfo {
  String name;
  String hostname;
  IPAddress ip;
  uint16_t port;
};

PrinterInfo printer;
bool printerFound = false;

// Image buffer (points to embedded data)
const uint8_t* imageBuffer = test_image_jpg;
size_t imageSize = test_image_jpg_len;

// ============================================================
// IPP Helper Functions
// ============================================================

void addIPPString(uint8_t*& ptr, uint8_t tag, const char* name, const char* value) {
  *ptr++ = tag;

  // Name length and name
  uint16_t nameLen = strlen(name);
  *ptr++ = (nameLen >> 8) & 0xFF;
  *ptr++ = nameLen & 0xFF;
  memcpy(ptr, name, nameLen);
  ptr += nameLen;

  // Value length and value
  uint16_t valueLen = strlen(value);
  *ptr++ = (valueLen >> 8) & 0xFF;
  *ptr++ = valueLen & 0xFF;
  memcpy(ptr, value, valueLen);
  ptr += valueLen;
}

// ============================================================
// Setup
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("  Quick Printer Test - Print Image");
  Serial.println("========================================\n");

  // Display image info
  Serial.println("--- Test Image Info ---");
  Serial.printf("Image size: %d bytes (embedded)\n", imageSize);
  Serial.printf("Magic bytes: %02X %02X %02X %02X\n",
                imageBuffer[0], imageBuffer[1], imageBuffer[2], imageBuffer[3]);

  bool isJPEG = (imageBuffer[0] == 0xFF && imageBuffer[1] == 0xD8);
  if (isJPEG) {
    Serial.println("Format: JPEG");
    Serial.println("✓ Image ready\n");
  } else {
    Serial.println("ERROR: Invalid image format!");
    return;
  }

  // Connect to WiFi
  if (!connectWiFi()) {
    Serial.println("ERROR: WiFi connection failed!");
    return;
  }

  // Discover printer
  if (!discoverPrinter()) {
    Serial.println("ERROR: No printer found!");
    Serial.println("Make sure printer is on and on same network");
    return;
  }

  // Query capabilities
  String supportedFormats;
  if (!getPrinterCapabilities(supportedFormats)) {
    Serial.println("WARNING: Could not query capabilities");
    supportedFormats = "octet-stream ";  // Fallback
  }

  // Print image
  printImage(supportedFormats);

  Serial.println("\n========================================");
  Serial.println("Test complete! Check your printer.");
  Serial.println("========================================\n");
}

void loop() {
  // Nothing - test runs once in setup()
  delay(10000);
}

// ============================================================
// Connect to WiFi
// ============================================================

bool connectWiFi() {
  Serial.println("\n--- Connecting to WiFi ---");
  Serial.printf("SSID: %s\n", WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ERROR: Connection timeout!");
    return false;
  }

  Serial.println("✓ WiFi connected");
  Serial.printf("IP: %s\n\n", WiFi.localIP().toString().c_str());
  return true;
}

// ============================================================
// Discover Printer via mDNS
// ============================================================

bool discoverPrinter() {
  Serial.println("\n--- Discovering Printer ---");

  if (!MDNS.begin("printer-test")) {
    Serial.println("ERROR: mDNS initialization failed!");
    return false;
  }

  const char* services[] = {"ipp", "ipps", "printer"};

  for (int i = 0; i < 3; i++) {
    Serial.printf("Querying _%s._tcp...", services[i]);
    Serial.flush();

    int n = MDNS.queryService(services[i], "tcp");
    Serial.printf(" found %d\n", n);

    if (n > 0) {
      printer.name = MDNS.hostname(0);
      printer.hostname = MDNS.hostname(0);
      printer.ip = MDNS.address(0);
      printer.port = MDNS.port(0);
      printerFound = true;

      Serial.println("\n✓ Printer found!");
      Serial.printf("  Name: %s\n", printer.name.c_str());
      Serial.printf("  IP: %s\n", printer.ip.toString().c_str());
      Serial.printf("  Port: %d\n\n", printer.port);

      return true;
    }
  }

  Serial.println("\nNo printer found");
  return false;
}

// ============================================================
// Get Printer Capabilities (with chunked decoding)
// ============================================================

bool getPrinterCapabilities(String& supportedFormats) {
  Serial.println("\n--- Querying Printer Capabilities ---");
  Serial.printf("Printer: %s:%d\n", printer.ip.toString().c_str(), printer.port);

  WiFiClient client;
  if (!client.connect(printer.ip, printer.port)) {
    Serial.println("ERROR: Connection failed!");
    return false;
  }

  String printerUri = "ipp://" + printer.ip.toString() + ":" + String(printer.port) + "/ipp/print";

  // Build IPP Get-Printer-Attributes request
  uint8_t ippRequest[512];
  uint8_t* ptr = ippRequest;

  // IPP version 1.1
  *ptr++ = 0x01;
  *ptr++ = 0x01;

  // Operation: Get-Printer-Attributes (0x000B)
  *ptr++ = 0x00;
  *ptr++ = 0x0B;

  // Request ID
  *ptr++ = 0x00;
  *ptr++ = 0x00;
  *ptr++ = 0x00;
  *ptr++ = 0x01;

  // Operation attributes group
  *ptr++ = 0x01;

  addIPPString(ptr, 0x47, "attributes-charset", "utf-8");
  addIPPString(ptr, 0x48, "attributes-natural-language", "en");
  addIPPString(ptr, 0x45, "printer-uri", printerUri.c_str());
  addIPPString(ptr, 0x44, "requested-attributes", "document-format-supported");

  // End of attributes
  *ptr++ = 0x03;

  int ippLen = ptr - ippRequest;

  // Send HTTP POST
  client.print("POST /ipp/print HTTP/1.1\r\n");
  client.print("Host: " + printer.ip.toString() + "\r\n");
  client.print("Content-Type: application/ipp\r\n");
  client.print("Content-Length: " + String(ippLen) + "\r\n");
  client.print("\r\n");
  client.write(ippRequest, ippLen);

  Serial.println("Request sent, waiting for response...");

  // Read response with proper 100 Continue handling
  String response = "";
  unsigned long timeout = millis() + 10000;
  bool gotFinalResponse = false;

  while (millis() < timeout && !gotFinalResponse) {
    while (client.available() == 0 && millis() < timeout) {
      delay(10);
    }

    if (millis() >= timeout) break;

    while (client.available()) {
      response += (char)client.read();
    }

    if (response.indexOf("HTTP/1.1 200") >= 0 || response.indexOf("HTTP/1.0 200") >= 0) {
      gotFinalResponse = true;
      break;
    }

    if (response.indexOf("100 Continue") >= 0) {
      Serial.println("  Got 100 Continue, waiting for final response...");
      delay(100);
      continue;
    }

    if (response.length() > 0) {
      delay(100);
    }
  }

  client.stop();

  if (!gotFinalResponse) {
    Serial.println("ERROR: Timeout or incomplete response");
    return false;
  }

  Serial.printf("Response received: %d bytes\n", response.length());

  // Handle HTTP 100 Continue followed by 200 OK
  int finalResponseStart = 0;
  if (response.indexOf("HTTP/1.1 100") >= 0 || response.indexOf("HTTP/1.0 100") >= 0) {
    Serial.println("  Skipping 100 Continue...");
    int secondHttp = response.indexOf("HTTP/1", 10);
    if (secondHttp >= 0) {
      finalResponseStart = secondHttp;
    }
  }

  // Find headers end
  int headerEnd = response.indexOf("\r\n\r\n", finalResponseStart);
  if (headerEnd == -1) {
    Serial.println("ERROR: Could not find end of headers");
    return false;
  }

  // Check if chunked
  String finalHeaders = response.substring(finalResponseStart, headerEnd);
  bool isChunked = (finalHeaders.indexOf("Transfer-Encoding: chunked") >= 0 ||
                    finalHeaders.indexOf("transfer-encoding: chunked") >= 0);

  int bodyStart = headerEnd + 4;

  Serial.printf("Headers end: %d, Chunked: %s\n", headerEnd, isChunked ? "YES" : "NO");

  // Decode chunks
  String ippData = "";
  if (isChunked) {
    Serial.println("Decoding chunks...");
    int pos = bodyStart;
    while (pos < response.length()) {
      int lineEnd = response.indexOf("\r\n", pos);
      if (lineEnd == -1) break;

      String chunkSizeStr = response.substring(pos, lineEnd);
      int chunkSize = strtol(chunkSizeStr.c_str(), NULL, 16);

      Serial.printf("  Chunk: 0x%s = %d bytes\n", chunkSizeStr.c_str(), chunkSize);

      if (chunkSize == 0) break;

      pos = lineEnd + 2;
      ippData += response.substring(pos, pos + chunkSize);
      pos += chunkSize + 2;
    }
  } else {
    ippData = response.substring(bodyStart);
  }

  Serial.printf("IPP data size: %d bytes\n", ippData.length());

  // Parse supported formats from binary IPP data
  // IPP encodes strings with length prefixes, so we need to search byte-by-byte
  supportedFormats = "";

  // Convert String to byte array for proper binary handling
  const uint8_t* ippBytes = (const uint8_t*)ippData.c_str();
  int ippDataLen = ippData.length();

  // Search for format strings in the binary IPP data
  for (int i = 0; i < ippDataLen - 10; i++) {
    // Look for mimeMediaType tag (0x49) followed by format strings
    if (ippBytes[i] == 0x49) {  // mimeMediaType tag
      // Skip name length (2 bytes) and name
      int nameLen = (ippBytes[i+1] << 8) | ippBytes[i+2];
      int valuePos = i + 3 + nameLen;

      if (valuePos + 2 < ippDataLen) {
        // Get value length
        int valueLen = (ippBytes[valuePos] << 8) | ippBytes[valuePos+1];
        valuePos += 2;

        if (valuePos + valueLen <= ippDataLen) {
          // Extract the format string
          String format = "";
          for (int j = 0; j < valueLen; j++) {
            format += (char)ippBytes[valuePos + j];
          }

          // Check which format it is
          if (format == "image/jpeg") {
            supportedFormats += "jpeg ";
            Serial.println("  ✓ image/jpeg");
          } else if (format == "image/png") {
            supportedFormats += "png ";
            Serial.println("  ✓ image/png");
          } else if (format == "image/gif") {
            supportedFormats += "gif ";
            Serial.println("  ✓ image/gif");
          } else if (format == "application/pdf") {
            supportedFormats += "pdf ";
            Serial.println("  ✓ application/pdf");
          } else if (format == "application/octet-stream") {
            supportedFormats += "octet-stream ";
            Serial.println("  ✓ application/octet-stream");
          } else if (format == "image/pwg-raster") {
            supportedFormats += "pwg-raster ";
            Serial.println("  ✓ image/pwg-raster");
          } else if (format == "image/urf") {
            supportedFormats += "urf ";
            Serial.println("  ✓ image/urf");
          }
        }
      }
    }
  }

  if (supportedFormats.length() == 0) {
    Serial.println("  ⚠ No formats detected, using fallback");
    supportedFormats = "octet-stream ";
  }

  Serial.println();
  return true;
}

// ============================================================
// Print Image
// ============================================================

bool printImage(const String& supportedFormats) {
  Serial.println("\n--- Printing Image ---");

  // Select format
  String documentFormat = "";
  if (supportedFormats.indexOf("jpeg") >= 0) {
    documentFormat = "image/jpeg";
  } else if (supportedFormats.indexOf("octet-stream") >= 0) {
    documentFormat = "application/octet-stream";
  } else if (supportedFormats.indexOf("pdf") >= 0) {
    documentFormat = "application/pdf";
  } else {
    documentFormat = "application/octet-stream";
  }

  Serial.printf("Selected format: %s\n", documentFormat.c_str());
  Serial.printf("Image size: %d bytes\n", imageSize);

  WiFiClient client;
  if (!client.connect(printer.ip, printer.port)) {
    Serial.println("ERROR: Connection failed!");
    return false;
  }

  String printerUri = "ipp://" + printer.ip.toString() + ":" + String(printer.port) + "/ipp/print";

  // Build IPP Print-Job request
  uint8_t ippRequest[1024];
  uint8_t* ptr = ippRequest;

  // IPP version 1.1
  *ptr++ = 0x01;
  *ptr++ = 0x01;

  // Operation: Print-Job (0x0002)
  *ptr++ = 0x00;
  *ptr++ = 0x02;

  // Request ID
  *ptr++ = 0x00;
  *ptr++ = 0x00;
  *ptr++ = 0x00;
  *ptr++ = 0x01;

  // Operation attributes group
  *ptr++ = 0x01;

  addIPPString(ptr, 0x47, "attributes-charset", "utf-8");
  addIPPString(ptr, 0x48, "attributes-natural-language", "en");
  addIPPString(ptr, 0x45, "printer-uri", printerUri.c_str());
  addIPPString(ptr, 0x42, "requesting-user-name", "ai-stick-test");
  addIPPString(ptr, 0x42, "job-name", "Test Print");
  addIPPString(ptr, 0x49, "document-format", documentFormat.c_str());

  // Job attributes group
  *ptr++ = 0x02;

  // End of attributes
  *ptr++ = 0x03;

  int ippHeaderSize = ptr - ippRequest;

  // Send HTTP POST
  Serial.println("Sending print job...");

  client.print("POST /ipp/print HTTP/1.1\r\n");
  client.print("Host: " + printer.ip.toString() + "\r\n");
  client.print("Content-Type: application/ipp\r\n");
  client.print("Content-Length: " + String(ippHeaderSize + imageSize) + "\r\n");
  client.print("\r\n");

  // Send IPP header
  client.write(ippRequest, ippHeaderSize);

  // Send image data in chunks
  const size_t chunkSize = 1024;
  size_t bytesSent = 0;
  while (bytesSent < imageSize) {
    size_t toSend = min(chunkSize, imageSize - bytesSent);
    client.write(imageBuffer + bytesSent, toSend);
    bytesSent += toSend;

    if (bytesSent % 10240 == 0) {  // Print progress every 10KB
      Serial.printf("  Sent: %d / %d bytes (%.0f%%)\n",
                    bytesSent, imageSize, (100.0 * bytesSent / imageSize));
    }
  }

  Serial.printf("  Sent: %d / %d bytes (100%%)\n", bytesSent, imageSize);

  // Read response
  Serial.println("Waiting for response...");

  String response = "";
  unsigned long timeout = millis() + 30000;
  bool gotFinalResponse = false;

  while (millis() < timeout && !gotFinalResponse) {
    while (client.available() == 0 && millis() < timeout) {
      delay(10);
    }

    if (millis() >= timeout) break;

    while (client.available()) {
      response += (char)client.read();
    }

    if (response.indexOf("HTTP/1.1 200") >= 0 || response.indexOf("HTTP/1.0 200") >= 0 ||
        response.indexOf("HTTP/1.1 4") >= 0 || response.indexOf("HTTP/1.1 5") >= 0) {
      gotFinalResponse = true;
      break;
    }

    if (response.indexOf("100 Continue") >= 0) {
      Serial.println("  Got 100 Continue...");
      delay(100);
      continue;
    }

    if (response.length() > 0) {
      delay(100);
    }
  }

  client.stop();

  if (!gotFinalResponse && millis() >= timeout) {
    Serial.println("ERROR: Response timeout!");
    return false;
  }

  Serial.println("\nResponse received:");
  Serial.println(response.substring(0, 500));

  bool success = (response.indexOf("HTTP/1.1 200") >= 0 || response.indexOf("HTTP/1.0 200") >= 0);

  if (success) {
    // Check for IPP errors
    if (response.indexOf("Invalid") >= 0 || response.indexOf("error") >= 0) {
      Serial.println("\n✗ Print job REJECTED by printer");
      return false;
    } else {
      Serial.println("\n✓ Print job submitted successfully!");
      return true;
    }
  } else {
    Serial.println("\n✗ Print job FAILED");
    return false;
  }
}
