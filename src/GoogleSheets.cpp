#include "GoogleSheets.h"
#include "google_service_account.h"
#include "WifiHandler.h"

const char GOOGLE_SA_PRIVATE_KEY_ARRAY[] PROGMEM = GOOGLE_SA_PRIVATE_KEY;

static SignerConfig signerConfig;
static volatile bool accessTokenInit = false;
static volatile bool accessTokenAvailable = false;
static volatile bool accessTokenRefreshing = false;
static volatile uint32_t tokenExpirySeconds = 30;
static volatile uint32_t tokenRefreshCount = 0;

bool isGoogleAccessTokenInit() {
    return accessTokenInit;
}

void signerGoogleAccessTokenStatusCallback(TokenInfo info) {
    if (info.status == esp_signer_token_status_error) {
        Serial.printf("Token info: type = %s, status = %s\n", Signer.getTokenType(info).c_str(), Signer.getTokenStatus(info).c_str());
        Serial.printf("Token error: %s\n", Signer.getTokenError(info).c_str());
    }
    else {
        Serial.printf("Token info: type = %s, status = %s\n", Signer.getTokenType(info).c_str(), Signer.getTokenStatus(info).c_str());
        if (info.status == esp_signer_token_status_ready) {
            Serial.printf("Token: %s\n", Signer.accessToken().c_str());
            tokenRefreshCount += 1;
            accessTokenAvailable = true;
            accessTokenRefreshing = false;
        }
    }
}

void setupGoogleAccessTokenAcquire(uint32_t expirySeconds) {

    /* Assign the sevice account credentials and private key (required) */
    signerConfig.service_account.data.client_email = GOOGLE_SA_CLIENT_EMAIL;
    signerConfig.service_account.data.project_id = GOOGLE_SA_PROJECT_ID;
    signerConfig.service_account.data.private_key = GOOGLE_SA_PRIVATE_KEY_ARRAY;

    /** Expired period in seconds (optional). 
     * Default is 3600 sec.
     * This may not afftect the expiry time of generated access token.
    */
    signerConfig.signer.expiredSeconds = 3600;

    /* Seconds to refresh the token before expiry time */
    signerConfig.signer.preRefreshSeconds = expirySeconds;
    tokenExpirySeconds = expirySeconds;

    /** Assign the API scopes (required) 
     * Use space or comma to separate the scope.
    */
    signerConfig.signer.tokens.scope = "https://www.googleapis.com/auth/drive.file, https://www.googleapis.com/auth/spreadsheets, https://www.googleapis.com/auth/userinfo.email";

    /** Assign the callback function for token generation status (optional) */
    signerConfig.token_status_callback = signerGoogleAccessTokenStatusCallback;

    /* Create token */
    tokenRefreshCount = 0;
    accessTokenRefreshing = true;
    Signer.begin(&signerConfig);

    accessTokenInit = true;
}

bool checkGoogleAccessTokenReady() {
    return checkGoogleAccessTokenReady(false);
}

bool checkGoogleAccessTokenReady(bool forceRefresh) {
  uint64_t expiryTime = Signer.getExpiredTimestamp();
  uint64_t timeNow = time(NULL);
  if(forceRefresh || expiryTime == 0 || (timeNow + tokenExpirySeconds) >= expiryTime) {
      accessTokenAvailable = false;
      accessTokenRefreshing = true;
      Signer.refreshToken();
  }
  return accessTokenAvailable;
}

String getGoogleAccessToken() {
  return checkGoogleAccessTokenReady() ? Signer.accessToken() : "";
}

bool appendRowToSheet(const char *sheetName, SheetDataItem *pSheetData, uint8_t columns) {

    bool sendOK = false;
    WiFiClientSecure *pClient = obtainSecureWifiClient();

    if (!isGoogleAccessTokenInit()) {
        Serial.println("Cannot append row - access token not available.");
        return false;
    }

    auto bearerToken = getGoogleAccessToken();

    if (bearerToken.length() == 0 || !checkGoogleAccessTokenReady()) {
        Serial.println("Cannot append row - access token not ready/will expire soon.");
        return false;
    }

    Serial.printf("Token OK - refreshed [%d] time(s).\n", tokenRefreshCount);

    const char *spreadSheetId = GOOGLE_SHEET_ID;
    char urlBuffer[256];
    sprintf(urlBuffer, "/v4/spreadsheets/%s/values/%s!A2:append?insertDataOption=INSERT_ROWS&valueInputOption=RAW", spreadSheetId, sheetName);
    if(!pClient->connect("sheets.googleapis.com", 443)) {
        Serial.printf("Cannot append row - cannot connect to Sheet - %d.\n", pClient->getWriteError());
        return false;
    }

    String payload = "{\"majorDimension\":\"ROWS\",\"range\":\"";
    payload += sheetName;
    payload += "!A2\",\"values\":[[";
    for(int i = 0 ; i < columns ; i++) {
        if(i > 0) {
            payload += ",";
        }
        if (!pSheetData[i].numeric) {
            payload += "\"";
        }
        payload += pSheetData[i].pValue;
        if (!pSheetData[i].numeric) {
            payload += "\"";
        }
    }
    payload += "]]}";
    Serial.print("Sending payload: ");
    Serial.println(payload);
    Serial.printf("To: %s", urlBuffer);

    // TODO: Sort out issue where we need to print stuff out for this to work; timing no doubt I think, so
    //       I think need to perhaps put delays in-between the sending of this and then wait a while
    //       before attempting to receive?
    pClient->setTimeout(10000);
    pClient->printf("POST %s HTTP/1.1\r\n", urlBuffer);
    int errorCode = pClient->getWriteError();
    if (errorCode == 0) {
        pClient->flush();
        errorCode = pClient->getWriteError();
    }
    else {
        printf("Failed sending initial request - error: %d\n", errorCode);
    }
    if (errorCode == 0) {
        pClient->printf("Host: %s\r\n", "sheets.googleapis.com");
        printf("Host: %s\r\n", "sheets.googleapis.com");
        errorCode = pClient->getWriteError();
        if (errorCode == 0) {
            pClient->flush();
            errorCode = pClient->getWriteError();
        }
        else {
            printf("Failed sending HOST error: %d\n", errorCode);
        }
    }
    if (errorCode == 0) {
        pClient->printf("Authorization: Bearer %s\r\n", bearerToken.c_str());
        printf("Authorization: Bearer %s\r\n", bearerToken.c_str());
        errorCode = pClient->getWriteError();
        if (errorCode == 0) {
            pClient->flush();
            errorCode = pClient->getWriteError();
        }
        else {
            printf("Failed sending Auth error: %d\n", errorCode);
        }
    }
    if (errorCode == 0) {
        pClient->print("Accept: */*\r\n");
        printf("Accept: */*\r\n");
        errorCode = pClient->getWriteError();
        if (errorCode == 0) {
            pClient->flush();
            errorCode = pClient->getWriteError();
        }
        else {
            printf("Failed sending Accept error: %d\n", errorCode);
        }
    }
    if (errorCode == 0) {
        pClient->print("Content-Type: application/json\r\n");
        printf("Content-Type: application/json\r\n");
        errorCode = pClient->getWriteError();
        if (errorCode == 0) {
            pClient->flush();
            errorCode = pClient->getWriteError();
        }
        else {
            printf("Failed sending Content-Type error: %d\n", errorCode);
        }
    }
    if (errorCode == 0) {
        pClient->print("User-Agent: ESP\r\n");
        printf("User-Agent: ESP\r\n");
        errorCode = pClient->getWriteError();
        if (errorCode == 0) {
            pClient->flush();
            errorCode = pClient->getWriteError();
        }
        else {
            printf("Failed sending User-Agent error: %d\n", errorCode);
        }
    }
    if (errorCode == 0) {
        pClient->printf("Content-Length: %d\r\n", payload.length());
        printf("Content-Length: %d\r\n", payload.length());
        errorCode = pClient->getWriteError();
        if (errorCode == 0) {
            pClient->flush();
            errorCode = pClient->getWriteError();
        }
        else {
            printf("Failed sending Content-Length error: %d\n", errorCode);
        }
    }
    if (errorCode == 0) {
        pClient->print("\r\n");
        printf("\r\n");
        errorCode = pClient->getWriteError();
        if (errorCode == 0) {
            pClient->flush();
            errorCode = pClient->getWriteError();
        }
        else {
            printf("Failed sending end of headers error: %d\n", errorCode);
        }
    }
    if (errorCode == 0) {
        pClient->print(payload.c_str());
        errorCode = pClient->getWriteError();
        printf(payload.c_str());
        if (errorCode == 0) {
            pClient->flush();
            errorCode = pClient->getWriteError();
        }
        else {
            printf("Failed sending body error: %d\n", errorCode);
        }
    }
    if (errorCode != 0) {
        Serial.printf("** Error when sending outbound: [%d]\n", errorCode);
        pClient->stop();
        return false;
    }
    // Wait a while for a response for up to 10 seconds...
    pClient->setTimeout(5000);
    Serial.print("Awaiting response...\n");
    String response = pClient->readStringUntil('\n');
    errorCode = pClient->getWriteError();

    if ( errorCode != 0 ) {
        Serial.printf("Error code [%d] - response: [%s]\n", errorCode, response.c_str());
        pClient->stop();
        return false;
    }
    Serial.printf("%s\n", response.c_str());

    if ( ( !response.startsWith("HTTP/1.1 ") && !response.startsWith("HTTP/1.0 " ) ) ) {
        Serial.printf("Did not receive status? [%s]\n", response.c_str());
        pClient->stop();
        return false;
    }

    Serial.printf("Received status - reading remainder: %s\n", response.c_str());
    for(int d = 0 ; d < 40 && pClient->available() == 0 && errorCode == 0 ; d++) {
        delay(250);
        errorCode = pClient->getWriteError();
    }

    if ( errorCode != 0 ) {
        Serial.printf("Error code [%d] - awaiting remainder.\n", errorCode);
        pClient->stop();
        return false;
    }

    int available = 0;
    do {
        available = pClient->available();
        errorCode = pClient->getWriteError();
        if (errorCode == 0) {
            if (available == 0) {
                delay(500);
                available = pClient->available();
            }
            if (available) {
                String dummy = pClient->readStringUntil('\n');
                Serial.printf("%s\n", dummy.c_str());
            }
        }
    }
    while ( available && errorCode == 0 );

    if ( errorCode != 0 ) {
        Serial.printf("Error code [%d] - reading remainder.\n", errorCode);
        pClient->stop();
        return false;
    }

    if( !response.startsWith("HTTP/1.1 ") && !response.startsWith("HTTP/1.0 ") ) {
        Serial.printf("Unable to parse response: %s Error: %d\n", response.c_str(), errorCode);
        pClient->stop();
        return false;
    }

    String responseCode = response.substring(9, 12);
    int returnCode = responseCode.toInt();
    if (returnCode == 200 || returnCode == 201) {
        Serial.println("Payload sent - OK");
        sendOK = true;
    }
    else {
        Serial.printf("Upload failed - response: %s\n", response.c_str());
    }
    pClient->stop();

    return sendOK;
}

bool updateRowInSheet(const char *sheetName, uint32_t rowNumber, uint32_t columnNumber, SheetDataItem *pSheetData, uint8_t columns) {

    bool sendOK = false;
    WiFiClientSecure *pClient = obtainSecureWifiClient();

    if (!isGoogleAccessTokenInit()) {
        Serial.println("Cannot update row - access token not available.");
        return false;
    }

    auto bearerToken = getGoogleAccessToken();

    if (bearerToken.length() == 0 || !checkGoogleAccessTokenReady()) {
        Serial.println("Cannot update row - access token not ready/will expire soon.");
        return false;
    }

    Serial.printf("Token OK - refreshed [%d] time(s).\n", tokenRefreshCount);

    const char *spreadSheetId = GOOGLE_SHEET_ID;
    char location[20] = { 0 };
    sprintf(location, "%c%d", char('A' + columnNumber), rowNumber + 1);
    char urlBuffer[256];
    sprintf(urlBuffer, "/v4/spreadsheets/%s/values/%s!%s?valueInputOption=RAW", spreadSheetId, sheetName, location);
    if(!pClient->connect("sheets.googleapis.com", 443)) {
        Serial.printf("Cannot update row - cannot connect to Sheet - %d.\n", pClient->getWriteError());
        return false;
    }

    String payload = "{\"majorDimension\":\"ROWS\",\"range\":\"";
    payload += sheetName;
    payload += "!";
    payload += location;
    payload += "\",\"values\":[[";
    for(int i = 0 ; i < columns ; i++) {
        if(i > 0) {
            payload += ",";
        }
        if (!pSheetData[i].numeric) {
            payload += "\"";
        }
        payload += pSheetData[i].pValue;
        if (!pSheetData[i].numeric) {
            payload += "\"";
        }
    }
    payload += "]]}";
    Serial.print("Sending payload: ");
    Serial.println(payload);
    Serial.printf("To: %s", urlBuffer);

    // TODO: Sort out issue where we need to print stuff out for this to work; timing no doubt I think, so
    //       I think need to perhaps put delays in-between the sending of this and then wait a while
    //       before attempting to receive?
    pClient->setTimeout(20000);
    pClient->printf("PUT %s HTTP/1.1\r\n", urlBuffer);
    int errorCode = pClient->getWriteError();
    if (errorCode == 0) {
        pClient->flush();
        errorCode = pClient->getWriteError();
    }
    else {
        printf("Failed sending initial request - error: %d\n", errorCode);
    }
    if (errorCode == 0) {
        pClient->printf("Host: %s\r\n", "sheets.googleapis.com");
        printf("Host: %s\r\n", "sheets.googleapis.com");
        errorCode = pClient->getWriteError();
        if (errorCode == 0) {
            pClient->flush();
            errorCode = pClient->getWriteError();
        }
        else {
            printf("Failed sending HOST error: %d\n", errorCode);
        }
    }
    if (errorCode == 0) {
        pClient->printf("Authorization: Bearer %s\r\n", bearerToken.c_str());
        printf("Authorization: Bearer %s\r\n", bearerToken.c_str());
        errorCode = pClient->getWriteError();
        if (errorCode == 0) {
            pClient->flush();
            errorCode = pClient->getWriteError();
        }
        else {
            printf("Failed sending Auth error: %d\n", errorCode);
        }
    }
    if (errorCode == 0) {
        pClient->print("Accept: */*\r\n");
        printf("Accept: */*\r\n");
        errorCode = pClient->getWriteError();
        if (errorCode == 0) {
            pClient->flush();
            errorCode = pClient->getWriteError();
        }
        else {
            printf("Failed sending Accept error: %d\n", errorCode);
        }
    }
    if (errorCode == 0) {
        pClient->print("Content-Type: application/json\r\n");
        printf("Content-Type: application/json\r\n");
        errorCode = pClient->getWriteError();
        if (errorCode == 0) {
            pClient->flush();
            errorCode = pClient->getWriteError();
        }
        else {
            printf("Failed sending Content-Type error: %d\n", errorCode);
        }
    }
    if (errorCode == 0) {
        pClient->print("Connection: close\r\n");
        printf("Connection: close\r\n");
        errorCode = pClient->getWriteError();
        if (errorCode == 0) {
            pClient->flush();
            errorCode = pClient->getWriteError();
        }
        else {
            printf("Failed sending Connection error: %d\n", errorCode);
        }
    }
    if (errorCode == 0) {
        pClient->print("User-Agent: ESP\r\n");
        printf("User-Agent: ESP\r\n");
        errorCode = pClient->getWriteError();
        if (errorCode == 0) {
            pClient->flush();
            errorCode = pClient->getWriteError();
        }
        else {
            printf("Failed sending User-Agent error: %d\n", errorCode);
        }
    }
    if (errorCode == 0) {
        pClient->printf("Content-Length: %d\r\n", payload.length());
        printf("Content-Length: %d\r\n", payload.length());
        errorCode = pClient->getWriteError();
        if (errorCode == 0) {
            pClient->flush();
            errorCode = pClient->getWriteError();
        }
        else {
            printf("Failed sending Content-Length error: %d\n", errorCode);
        }
    }
    if (errorCode == 0) {
        pClient->print("\r\n");
        printf("\r\n");
        errorCode = pClient->getWriteError();
        if (errorCode == 0) {
            pClient->flush();
            errorCode = pClient->getWriteError();
        }
        else {
            printf("Failed sending end of headers error: %d\n", errorCode);
        }
    }
    if (errorCode == 0) {
        printf(payload.c_str());
        pClient->print(payload.c_str());
        errorCode = pClient->getWriteError();
        if (errorCode == 0) {
            pClient->flush();
            errorCode = pClient->getWriteError();
        }
        else {
            printf("Failed sending body error: %d\n", errorCode);
        }
    }
    if (errorCode != 0) {
        Serial.printf("** Error when sending outbound: [%d]\n", errorCode);
        pClient->stop();
        return false;
    }
    // Wait a while for a response for up to 10 seconds...
    Serial.print("Awaiting response...\n");
    String response = pClient->readStringUntil('\n');
    if (response.length() == 0) {
        response = pClient->readStringUntil('\n');
    }
    errorCode = pClient->getWriteError();

    if ( errorCode != 0 ) {
        Serial.printf("Error code [%d] - response: [%s]\n", errorCode, response.c_str());
        pClient->stop();
        return false;
    }

    if ( ( !response.startsWith("HTTP/1.1 ") && !response.startsWith("HTTP/1.0 " ) ) ) {
        Serial.printf("Did not receive status? [%s]\n", response.c_str());
        pClient->stop();
        // WORKAROUND: We do not always receive a response; to stop duplicated entries, assume all is well...
        return response.length() == 0;
    }

    Serial.printf("Received status - reading remainder: %s\n", response.c_str());
    for(int d = 0 ; d < 40 && pClient->available() == 0 && errorCode == 0 ; d++) {
        delay(250);
        errorCode = pClient->getWriteError();
    }

    if ( errorCode != 0 ) {
        Serial.printf("Error code [%d] - awaiting remainder.\n", errorCode);
        pClient->stop();
        return false;
    }

    int available = 0;
    do {
        available = pClient->available();
        errorCode = pClient->getWriteError();
        if (errorCode == 0) {
            if (available == 0) {
                delay(500);
                available = pClient->available();
            }
            if (available) {
                String dummy = pClient->readStringUntil('\n');
                printf("%s\n", dummy.c_str());
            }
        }
    }
    while ( available && errorCode == 0 );

    if ( errorCode != 0 ) {
        Serial.printf("Error code [%d] - reading remainder.\n", errorCode);
        pClient->stop();
        return false;
    }

    if( !response.startsWith("HTTP/1.1 ") && !response.startsWith("HTTP/1.0 ") ) {
        Serial.printf("Unable to parse response: %s Error: %d\n", response.c_str(), errorCode);
        pClient->stop();
        return false;
    }

    String responseCode = response.substring(9, 12);
    int returnCode = responseCode.toInt();
    if (returnCode == 200 || returnCode == 201) {
        Serial.println("Payload sent - OK");
        sendOK = true;
    }
    else {
        Serial.printf("Upload failed - response: %s\n", response.c_str());
    }
    pClient->stop();

    return sendOK;
}
