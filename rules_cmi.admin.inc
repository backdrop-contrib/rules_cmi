<?php
/**
 * @file
 *  Contains function for the administration of Rules CMI.
 */

/**
 * Form builder for main Rules CMI configuration.
 */
function rules_cmi_configuration_form($form, $form_state) {
  $help_items[] = t('Selecting <i>Rules stored as CMI json files</i> will cause all new Rules to be stored as CMI json files in the active config directory.');
  $help_items[] = t('Selecting <i>Rules stored in database</i> will cause all new Rules to be stored in the database.');
  $help = theme('item_list', array('items' => $help_items));
  $help .= t('<p>Changing storage type will NOT automatically transfer existing Rules. To transfer existing Rules, please visit <a href="@transfer_link">Rules transfer</a>.</p>', array('@transfer_link' => url('admin/config/workflow/rules_cmi/transfer')));
  $form['explanation'] = array(
    '#type' => 'fieldset',
    '#title' => t('About Rules CMI Storage'),
    'help' => array(
      '#type' => 'help',
      '#markup' => $help,
    ),
  );
  $form['cmi_storage_status'] = array(
    '#type' => 'radios',
    '#title' => 'Rules storage',
    '#options' => array(
      'cmi' => t('Rules stored as CMI json files'),
      'database' => t('Rules stored in database'),
    ),
    '#default_value' => config_get('rules_cmi.settings', 'cmi_storage_status'),
  );
  $form['actions'] = array(
    '#type' => 'actions',
    'submit' => array(
      '#type' => 'submit',
      '#value' => t('Save'),
    )
  );
  return $form;
}

function rules_cmi_configuration_form_submit($form, &$form_state) {
  $status = $form_state['values']['cmi_storage_status'];
  config_set('rules_cmi.settings', 'cmi_storage_status', $status);
  $status_message = $status == 'cmi' ? t('Rules are stored as CMI json files') : t('Rules are stored in database');
  backdrop_set_message($status_message);
  entity_info_cache_clear();
}

/**
 * Form builder for transfer from database to CMI.
 */
function rules_cmi_transfer_to_cmi_form($form, &$form_state) {
  $q = db_query('SELECT COUNT(id) FROM {rules_config}');
  $rules_count = $q->fetchField();
  if ($rules_count < 1) {
    $form['count'] = array(
      '#markup' => t('There are no Rules currently stored in database.'),
    );
  }
  else {
    $form['count'] = array(
      '#type' => 'help',
      '#markup' => format_plural($rules_count, 'There is @count Rule currently stored in database.', 'There are @count Rules currently stored in database.', array($rules_count)),
    );
    if (config_get('rules_cmi.settings', 'cmi_storage_status') != 'cmi') {
      backdrop_set_message(t('The database Rules cannot be transferred to CMI. Be sure to select "Rules stored as CMI json files" in the <a href="@settings-url">Rules CMI Status page</a> and try again.', array('@settings-url' => url('admin/config/workflow/rules_cmi'))), 'warning');
      $form['note'] = array(
        '#type' => 'help',
        '#markup' => t('Rules cannot be transferred.'),
      );
    }
    else {
      $form['transfer_options'] = array(
        '#type' => 'fieldset',
        '#title' => t('Transfer options'),
      );
      $form['transfer_options']['remove'] = array(
        '#type' => 'checkbox',
        '#title' => t('Delete the database Rules after transfer'),
        '#description' => t('Warning: this action cannot be undone'),
      );
      $form['transfer_options']['if_exists'] = array(
        '#type' => 'radios',
        '#title' => t('If a Rule with the same machine name exists in CMI'),
        '#options' => array(
          'skip' => t('Do not transfer the Rule if a CMI Rule with the same name exists'),
          'overwrite' => t('Overwrite the existing CMI Rule'),
          'clone' => t('Create a new Rule with a modified machine name'),
        ),
        '#default_value' => 'skip',
        '#required' => TRUE,
      );
      $form['warning'] = array(
        '#type' => 'help',
        '#markup' => 'Warning: this action cannot be undone.'
      );
      $form['actions'] = array(
        '#type' => 'actions',
        'submit' => array(
          '#type' => 'submit',
          '#value' => t('Transfer database Rules to CMI storage'),
        ),
      );
    }
  }
  return $form;
}

/**
 * Submit handler.
 */
function rules_cmi_transfer_to_cmi_form_submit($form, &$form_state) {
  $remove = $form_state['values']['remove'];
  $if_exists = $form_state['values']['if_exists'];
  // Get all existing CMI rule names.
  $existing_names = config_get_names_with_prefix('rules_config.');
  $existing_names = str_replace('rules_config.', '', $existing_names);
  // Get all database Rules.
  $q = db_query('SELECT rules_config.* FROM {rules_config} rules_config');
  $database_rules = $q->fetchAll();
  $batch = array(
    'operations' => array(
      array('rules_cmi_transfer_to_cmi_batch', array($database_rules, $existing_names, $if_exists, $remove)),
    ),
    'finished' => 'rules_cmi_transfer_to_cmi_batch_finished',
    'title' => t('Transferring Rules from database to CMI'),
    'init_message' => t('Process is starting.'),
    'progress_message' => t('Processed @current out of @total.'),
    'error_message' => t('The process has encountered an error.'),
    'file' => backdrop_get_path('module', 'rules_cmi') . '/rules_cmi.admin.inc',
  );
  batch_set($batch);
}

/**
 * Batch callback to transfer from database to CMI.
 */
function rules_cmi_transfer_to_cmi_batch($database_rules, $existing_cmi_names, $if_name_exists, $remove_rule, &$context) {
  if (!isset($context['sandbox']['progress'])) {
    $context['sandbox']['database_rules'] = $database_rules;
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['current_item'] = 0;
    $context['sandbox']['total'] = count($database_rules);
  }
  // How many items to process in each call.
  $total_to_process = 10;
  while ($total_to_process > 0 && $context['sandbox']['current_item'] < $context['sandbox']['total']) {
    $current_index = $context['sandbox']['current_item'];
    $database_rule = $context['sandbox']['database_rules'][$current_index];
    $original_rule_id = $database_rule->id;
    switch ($if_name_exists) {
      case 'skip':
        if (in_array($database_rule->name, $existing_cmi_names)) {
          $context['sandbox']['current_item']++;
          $context['sandbox']['progress']++;
          $total_to_process--;
          continue 2;
        }
        break;
      case 'clone':
        if (in_array($database_rule->name, $existing_cmi_names)) {
          $database_rule->name = _rule_cmi_unique_name($database_rule->name, $existing_cmi_names);
          $database_rule->label = $database_rule->label . ' ' . t('(from db)');
        }
        break;
    }
    _rules_cmi_transfer_to_cmi_helper($database_rule);
    if ($remove_rule) {
      db_delete('rules_config')
        ->condition('id', $original_rule_id)
        ->execute();
      db_delete('rules_dependencies')
        ->condition('id', $original_rule_id)
        ->execute();
      db_delete('rules_tags')
        ->condition('id', $original_rule_id)
        ->execute();
      db_delete('rules_trigger')
        ->condition('id', $original_rule_id)
        ->execute();    
    }

    $context['message'] = t('Now processing Rule %item', array('%item' => $database_rule->label));
    $context['results'][] = t('Transferred %item', array('%item' => $database_rule->name));
    $total_to_process--;
    $context['sandbox']['current_item']++;
    $context['sandbox']['progress']++;
  }
  if ($context['sandbox']['progress'] != $context['sandbox']['total']) {
    $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['total'];
  }
}

/**
 * Transfer to CMI finish callback.
 */
function rules_cmi_transfer_to_cmi_batch_finished($success, $results, $operations) {
  if ($success) {
    $message = t('@count items successfully processed.', array('@count' => count($results)));;
    backdrop_set_message($message);  }
  else {
    // An error occurred.
    // $operations contains the operations that remained unprocessed.
    $error_operation = reset($operations);
    $message = t('An error occurred while processing %error_operation with arguments: @arguments', array('%error_operation' => $error_operation[0], '@arguments' => print_r($error_operation[1], TRUE)));
    backdrop_set_message($message, 'error');
  }
}

/**
 * Transfer from database to CMI helper. 
 */
function _rules_cmi_transfer_to_cmi_helper($record) {
  $rules_plugin = unserialize($record->data);
  // Get tags
  $q = db_select('rules_tags');
  $q->condition('id', $record->id);
  $q->addField('rules_tags', 'tag');
  $tags = $q->execute()->fetchCol();
  // Get dependencies.
  $q = db_select('rules_dependencies');
  $q->condition('id', $record->id);
  $q->addField('rules_dependencies', 'module');
  $dependencies = $q->execute()->fetchCol();

  $properties = array('name', 'label', 'plugin', 'active', 'weight', 'status', 'dirty', 'module', 'owner', 'access_exposed', 'data');
  foreach ($properties as $property) {
    $rules_plugin->$property = $record->$property;
  }
  $rules_plugin->tags = $tags;
  $rules_plugin->dependencies = $dependencies;
  $rules_plugin->save();
}

/**
 * Helper to find a unique name for the CMI rule.
 */
function _rule_cmi_unique_name($database_rule_name, $existing_cmi_names) {
  $chosen_name = $database_rule_name;
  $i = 1;
  while (in_array($chosen_name, $existing_cmi_names)) {
    // Ensure the username does not exceed the maximum character limit.
    // The hard limit for the name is 64, but we also need to account for
    // the rules_config prefix.
    if (strlen($database_rule_name . '_' . $i) > 51) {
      $database_rule_name = substr($database_rule_name, 0, strlen($database_rule_name) - strlen('_' . $i));
    }
    $chosen_name = $database_rule_name . '_' . $i++;
  }
  return $chosen_name;
}

/**
 * Form builder.
 * 
 * Transfer rules from cmi to db.
 */
function rules_cmi_transfer_to_db_form($form, &$form_state) {
  $existing_cmi_rule_names = config_get_names_with_prefix('rules_config.');

  if (count($existing_cmi_rule_names) < 1) {
    $form['count'] = array(
      '#markup' => t('There are no Rules currently in CMI storage.'),
    );
  }
  else {
    $form['count'] = array(
      '#type' => 'help',
      '#markup' => format_plural(count($existing_cmi_rule_names), 'There is @count Rule currently stored in database.', 'There are @count Rules in CMI storage.', array((count($existing_cmi_rule_names)))),
    );
    if (config_get('rules_cmi.settings', 'cmi_storage_status') != 'database') {
      backdrop_set_message(t('The CMI Rules cannot be transferred to database. Be sure to select "Rules stored in database" in the <a href="@settings-url">Rules CMI Status page</a> and try again.', array('@settings-url' => url('admin/config/workflow/rules_cmi'))), 'warning');
      $form['note'] = array(
        '#type' => 'help',
        '#markup' => t('Rules cannot be transferred.'),
      );
    }
    else {
      $form['transfer_options'] = array(
        '#type' => 'fieldset',
        '#title' => t('Transfer options'),
      );
      $form['transfer_options']['remove'] = array(
        '#type' => 'checkbox',
        '#title' => t('Delete the CMI Rules after transfer'),
        '#description' => t('Warning: this action cannot be undone'),
      );
      $form['transfer_options']['if_exists'] = array(
        '#type' => 'radios',
        '#title' => t('If a Rule with the same machine name exists in database'),
        '#options' => array(
          'skip' => t('Do not transfer the Rule if a database Rule with the same name exists'),
          'overwrite' => t('Overwrite the existing database Rule'),
          'clone' => t('Create a new Rule with a modified machine name'),
        ),
        '#default_value' => 'skip',
        '#required' => TRUE,
      );
      $form['warning'] = array(
        '#type' => 'help',
        '#markup' => 'Warning: this action cannot be undone.'
      );
      $form['actions'] = array(
        '#type' => 'actions',
        'submit' => array(
          '#type' => 'submit',
          '#value' => t('Transfer CMI Rules to database storage'),
        ),
      );
    }
  }
  return $form;
}


/**
 * Submit handler.
 */
function rules_cmi_transfer_to_db_form_submit($form, &$form_state) {
  $remove = $form_state['values']['remove'];
  $if_exists = $form_state['values']['if_exists'];
  // Get all existing database rule names.
  $q = db_select('rules_config');
  $q->addField('rules_config', 'name');
  $existing_names = $q->execute()->fetchCol();
  // Get all CMI Rule names.
  $cmi_rule_names = config_get_names_with_prefix('rules_config.');
  $batch = array(
    'operations' => array(
      array('rules_cmi_transfer_to_database_batch', array($cmi_rule_names, $existing_names, $if_exists, $remove)),
    ),
    'finished' => 'rules_cmi_transfer_to_database_batch_finished',
    'title' => t('Transferring Rules from CMI to database'),
    'init_message' => t('Process is starting.'),
    'progress_message' => t('Processed @current out of @total.'),
    'error_message' => t('The process has encountered an error.'),
    'file' => backdrop_get_path('module', 'rules_cmi') . '/rules_cmi.admin.inc',
  );
  batch_set($batch);
}


/**
 * Batch callback to transfer from database to CMI.
 */
function rules_cmi_transfer_to_database_batch($cmi_rule_names, $existing_names, $if_name_exists, $remove_rule, &$context) {
  if (!isset($context['sandbox']['progress'])) {
    $context['sandbox']['cmi_rule_names'] = $cmi_rule_names;
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['current_item'] = 0;
    $context['sandbox']['total'] = count($cmi_rule_names);
  }
  // How many items to process in each call.
  $total_to_process = 10;
  while ($total_to_process > 0 && $context['sandbox']['current_item'] < $context['sandbox']['total']) {
    $current_index = $context['sandbox']['current_item'];
    $cmi_rule_name = $context['sandbox']['cmi_rule_names'][$current_index];
    $config = config($cmi_rule_name);
    $cmi_rule_array = $config->get();

    if (in_array($cmi_rule_array['name'], $existing_names)) {
      switch ($if_name_exists) {
        case 'skip':
          $context['sandbox']['current_item']++;
          $context['sandbox']['progress']++;
          $total_to_process--;
          continue 2;
          break;
        case 'clone':
          $cmi_rule_array['name'] = _rule_cmi_unique_name($cmi_rule_array['name'], $existing_names);
          $cmi_rule_array['label'] = $cmi_rule_array['label'] . ' ' . t('(from cmi)');
          break;
        case 'overwrite':
          $db_rules = db_select('rules_config')->fields('rules_config', array())->condition('name', $cmi_rule_array['name'])->execute()->fetchAll();
          $id = $db_rules[0]->id;
          db_delete('rules_config')
            ->condition('id', $id)->execute();
          db_delete('rules_tags')
            ->condition('id', $id)->execute();
          db_delete('rules_trigger')
            ->condition('id', $id)->execute();
          db_delete('rules_dependencies')
            ->condition('id', $id)->execute();
          break;
      }
    }
    
    _rules_cmi_transfer_to_database_helper($cmi_rule_array);
    if ($remove_rule) {
      $config->delete();   
    }

    $context['message'] = t('Now processing Rule %item', array('%item' => $cmi_rule_array['label']));
    $context['results'][] = t('Transferred %item', array('%item' => $cmi_rule_array['name']));
    $total_to_process--;
    $context['sandbox']['current_item']++;
    $context['sandbox']['progress']++;
  }
  if ($context['sandbox']['progress'] != $context['sandbox']['total']) {
    $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['total'];
  }
}

/**
 * Transfer to CMI finish callback.
 */
function rules_cmi_transfer_to_database_batch_finished($success, $results, $operations) {
  if ($success) {
    $message = t('@count items successfully processed.', array('@count' => count($results)));;
    backdrop_set_message($message);  }
  else {
    // An error occurred.
    // $operations contains the operations that remained unprocessed.
    $error_operation = reset($operations);
    $message = t('An error occurred while processing %error_operation with arguments: @arguments', array('%error_operation' => $error_operation[0], '@arguments' => print_r($error_operation[1], TRUE)));
    backdrop_set_message($message, 'error');
  }
}

/**
 * Helper to write the database rule.
 */
function _rules_cmi_transfer_to_database_helper($cmi_rule_array) {
  $dependencies = $cmi_rule_array['dependencies'];
  unset($cmi_rule_array['dependencies']);
  $tags = $cmi_rule_array['tags'];
  unset($cmi_rule_array['tags']);
  unset($cmi_rule_array['id']);
  $rules_plugin = unserialize($cmi_rule_array['data']);
  $cmi_rule_array['data'] = $rules_plugin;
  backdrop_write_record('rules_config', $cmi_rule_array);
  $id = $cmi_rule_array['id'];
  
  // Write to the rules_dependencies table.
  foreach ($dependencies as $dependency) {
    db_merge('rules_dependencies')
      ->key(array('id' => $id, 'module' => $dependency))
      ->fields(array(
        'id' => $id,
        'module' => $dependency,
    ))->execute();
  }
  // Write to the rules_tags table.
  foreach ($tags as $tag) {
    db_merge('rules_tags')
      ->key(array('id' => $id, 'tag' => $tag))
      ->fields(array(
        'id' => $id,
        'tag' => $tag,
    ))->execute();
  }
  // Write to the rules_trigger table.
  if ($rules_plugin instanceof RulesTriggerableInterface) {
    $events = $rules_plugin->events();
    foreach ($events as $event) {
      db_merge('rules_trigger')
        ->key(array('id' => $id, 'event' => $event))
        ->fields(array(
          'id' => $id,
          'event' => $event,
      ))->execute();
    }
  }
}
