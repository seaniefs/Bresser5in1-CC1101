- Create service account:

 Name: `<<The name you decide>>`
 
 Desc: `<<An appropriate description - E.g. Automated Upload for Weather Information>>`

 No need to assign any roles

When created, go to the service account and select the "Manage Keys" option and create a new key and download the JSON for it.

_When downloaded create a file called google_service_account.h in the project:_

```
#ifndef _GOOGLE_SERVICE_ACCOUNT_H_

  #define _GOOGLE_SERVICE_ACCOUNT_H_ 1

  #define GOOGLE_SA_PROJECT_ID "<< project_id from json>>"
  #define GOOGLE_SA_CLIENT_EMAIL "<< client_email from json >>"
  #define GOOGLE_SA_PRIVATE_KEY "<< private_key from json >>"
  #define GOOGLE_SHEET_ID       "<< google sheet id >>"

#endif
```

- Create a folder in the drive then give access to it to the service account

- Need to enable this API for the project this account belongs to:

   * APIS and Services 
   * -> Enable APIS and Services
   * -> Google Sheets API

Once you have done this - create a sheet within the folder and take the id from this and populate this into the GOOGLE_SHEET_ID value of _GOOGLE_SERVICE_ACCOUNT_H_.

TODO: Make a web interface so as this is all configurable.