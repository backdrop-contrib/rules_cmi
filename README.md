# Rules CMI Storage

An EXPERIMENTAL module that changes the storage of Rules from database tables to configuration CMI
JSON files. This means you can export/import Rules the same way you export/import other 
configuration files: through the Configuration Manager in the admin menu.

## How to use
- Enable the module
- By default, this modules does NOT enable CMI storage. You need to manually enable CMI storage by visiting the configuration page at `admin/config/workflow/Rules_cmi`
- If you had Rules stored in the database, you can transfer them to CMI at the module's configuration page
- Create and save, or edit existing Rules as normal
- To export Rules as CMI json code, head to Backdrop's Configuration Manager at `admin/config/development/configuration/single/export`. You can still export Rules the "old way" from the UI, but those exports will not be CMI json code (they will still work to import)

### What this module can do at this point
- Save new Rules as CMI json files, meaning you can import/export them as other config files
- Edit and update Rules that have been created AFTER this module was enabled
- Delete Rules
- Clone Rules
- Use the UI operation dropbutton to go to Configuration Manager to export the Rule CMI
- Import Rules exported as CMI (through Configuration Manager)
- Import Rules exported through the Rules UI (non-CMI json code) 
- Search Rules by tags and events
- Transfer Rules from CMI storage to database storage and vice versa

### What this module can't yet do
- Autocompleting rule tags doesn't yet work

## Credits
- Created and maintained by [argiepiano](https://github.com/argiepiano)

## License
This project is GPL v2 software. 
See the LICENSE.txt file in this directory for complete text.
