# Rules CMI Storage

An EXPERIMENTAL module that allows the saving of Rules configurations as CMI
JSON files. This means you can export/import rules the same way you import/export other 
configuration files (through the Configuration Manager in the admin menu).

USE AT YOUR OWN RISK.

This module has not been thoroughly tested. If you are interested, please DO TEST. I'd like to make this module fully functional, thus  

### What this module can do at this point
- Save new rules as CMI json files, meaning you can import/export them as other config files
- Edit and update rules that have been created AFTER this module was enabled
- Delete rules
- Search rules by tags and events

### What this module can't yet do
- It can't move existing rules before the module is enabled
- It can't move rules "back" to regular storing when the module is disabled (rules will be lost)
- Autocompleting rule tags doesn't yet work

### What needs to be tested
- Cloning rules
- Importing rules

## Credits
- Created and maintained by [argiepiano](https://github.com/argiepiano)

## License
This project is GPL v2 software. 
See the LICENSE.txt file in this directory for complete text.