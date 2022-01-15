#ifndef _GOOGLE_SHEETS_H_

    #define _GOOGLE_SHEETS_H_ 1

    #include <stdbool.h>
    #include <ESPSigner.h>

    typedef struct SheetDataItem {
        const char *pValue;
        bool  numeric;
    } SheetDataItem;

    bool isGoogleAccessTokenInit();
    void signerGoogleAccessTokenStatusCallback(TokenInfo info);
    void setupGoogleAccessTokenAcquire(uint32_t expirySeconds);
    bool checkGoogleAccessTokenReady();
    bool checkGoogleAccessTokenReady(bool forceRefresh);
    String getGoogleAccessToken();

    bool appendRowToSheet(SheetDataItem *pSheetData, uint8_t columns);

#endif