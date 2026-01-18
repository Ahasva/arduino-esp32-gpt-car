#include "fs_web.h"
#include <LittleFS.h>

static String getContentType(WebServer& server, const String& filename) {
  if (server.hasArg("download")) return "application/octet-stream";
  if (filename.endsWith(".htm") || filename.endsWith(".html")) return "text/html";
  if (filename.endsWith(".css")) return "text/css";
  if (filename.endsWith(".js")) return "application/javascript";
  if (filename.endsWith(".png")) return "image/png";
  if (filename.endsWith(".gif")) return "image/gif";
  if (filename.endsWith(".jpg")) return "image/jpeg";
  if (filename.endsWith(".ico")) return "image/x-icon";
  if (filename.endsWith(".xml")) return "text/xml";
  if (filename.endsWith(".pdf")) return "application/pdf";
  if (filename.endsWith(".zip")) return "application/zip";
  if (filename.endsWith(".gz"))  return "application/gzip";
  return "text/plain";
}

bool handleFileRead(WebServer& server, const String& inPath) {
  String path = inPath;
  if (path.endsWith("/")) path += "index.html";

  if (!LittleFS.exists(path)) return false;

  File file = LittleFS.open(path, "r");
  if (!file) return false;

  server.streamFile(file, getContentType(server, path));
  file.close();
  return true;
}

void listFiles() {
  Serial.println("Files on FS:");
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while (file) {
    Serial.print("  ");
    Serial.println(file.name());
    file = root.openNextFile();
  }
}