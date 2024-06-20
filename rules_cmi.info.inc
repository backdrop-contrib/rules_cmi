<?php

/**
 * Implements hook_entity_property_info_alter().
 */
function rules_cmi_entity_property_info_alter(&$info) {
  $properties = &$info['rules_config']['properties'];

  // Define dependencies. These will be stored per entity as opposed to a
  // separate table.
  $properties['dependencies'] = array(
    'type' => 'struct',
    'label' => t('Dependencies'),
    'description' => t('Stores an array of modules this rule depends on.'),
  );

  // Define tags. These will be stored per entity as opposed to a separate table.
  $properties['tags'] = array(
    'type' => 'struct',
    'label' => t('Tags'),
    'description' => t('Stores an array of tags for this rule configuration.'),
  );

  $properties['plugin'] = array(
    'type' => 'token',
    'label' => t('Plugin'),
    'description' => t('Stores the plugin type.'),
  );

  $properties['data'] = array(
    'type' => 'text',
    'label' => t('Data'),
    'description' => t('Data'),
  );
}

