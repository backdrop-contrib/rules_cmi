# Rules CMI Storage

An EXPERIMENTAL module that changes the storage of Rules from database tables to configuration CMI
JSON files. This means you can export/import rules the same way you export/import other 
configuration files: through the Configuration Manager in the admin menu.

This module still needs testing. If you are interested, please DO TEST and open issues in the issue queue. I'd like to make this module fully functional.

## How to use
At this point, simply enable the module and create and save rules. NB: this version still cannot move Rules from database to CMI and vice versa. But you can use the default non-CMI json export and import functionality provided by Rules to export all your rules and then import them after enabling the module. 

### What this module can do at this point
- Save new rules as CMI json files, meaning you can import/export them as other config files
- Edit and update rules that have been created AFTER this module was enabled
- Delete rules
- Clone rules
- Use the UI operation dropbutton to go to Configuration Manager to export the Rule CMI
- Import rules exported as CMI (through Configuration Manager)
- Import rules exported through the Rules UI (non-CMI json code) 
- Search rules by tags and events

### What this module can't yet do
- It can't move existing rules before the module is enabled
- It can't move rules "back" to regular storing when the module is disabled (rules will be lost)
- Autocompleting rule tags doesn't yet work

## Credits
- Created and maintained by [argiepiano](https://github.com/argiepiano)

## License
This project is GPL v2 software. 
See the LICENSE.txt file in this directory for complete text.
