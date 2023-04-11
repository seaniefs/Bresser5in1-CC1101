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
    // TODO: Need row number - column number A = 0, Z = 25 - AA = 26 - set of sheet data items for x columns; contiguous
    bool updateRowInSheet(const char *sheetName, uint32_t rowNumber, uint32_t columnNumber, SheetDataItem *pSheetData, uint8_t columns);
    bool appendRowToSheet(const char *sheetName, SheetDataItem *pSheetData, uint8_t columns);

#endif