<?php
/**
 * @file
 *  Contains class RulesEntityCmiController, which overrides
 *  EntityPlusCmiController and incorporates methods from RulesEntityController.
 */

/**
 * A controller that is used instead of RulesEntityController.
 * 
 * Saves rules_config entities as CMI json files.
 */
class RulesEntityCmiController extends EntityPlusCmiController {

  /**
   * Overridden.
   * 
   * This is an adaptation of RulesEntityController::create()
   */
  public function create(array $values = array()) {
    // $values += array('owner' => 'rules', 'status' => ENTITY_PLUS_CUSTOM);
    $rules_config = parent::create($values);
    $this->setDefaults($rules_config);
    return $rules_config;
  }

  /**
   * Overridden.
   * 
   * @see DefaultEntityController::attachLoad()
   */
  protected function attachLoad(&$retrieved_entities, $revision_id = FALSE) {
    $entities = array();
    foreach ($retrieved_entities as $record) {
      $entity = unserialize($record->data);
      // Set the values of the other properties.
      // Get the list of defined properties. All properties must be defined
      // by hook_entity_property_info(). 
      $properties = entity_plus_get_property_info($this->entityType);
      // Property events requires special handling.
      unset($properties['properties']['events']);
      foreach ($properties['properties'] as $field_name => $info) {
        $entity->$field_name = $record->$field_name;
      }

      unset($entity->data);
      $entities[$entity->name] = $entity;
    }
    $retrieved_entities = $entities;
    parent::attachLoad($retrieved_entities, $revision_id);
  }

  /**
   * Override to support having events and tags as conditions.
   * 
   * This is an adaptation of RulesEntityController::applyConditions()
   *
   * @see EntityPlusControllerExportable::applyConditions()
   */
  protected function applyConditions($entities, $conditions = array()) {
    if (isset($conditions['event'])) {
      foreach ($entities as $key => $entity) {
        // Event conditions are special. Retrieve the events from the serialized
        // RulesPlugin object if our entity is not a plugin.
        if (is_a($entity, 'EntityPlusCmiEntity')) {
          $rules_object = unserialize($entity->data);
        }
        else {
          $rules_object = $entity;
        }
        if ($rules_object instanceof RulesTriggerableInterface) {
          if (!in_array($conditions['event'], $rules_object->events())) {
            $trimmed_condition = trim($conditions['event'], '%');
            $found = FALSE;
            foreach ($rules_object->events() as $event) {
              if (strpos($event, $trimmed_condition) !== FALSE) {
                $found = TRUE;
                break;
              }
            }
            if ($found == FALSE) {
              unset($entities[$key]);
            }
          }
        }
      }
      unset($conditions['event']);
    }
    if (isset($conditions['plugin'])) {
      foreach ($entities as $key => $entity) {
        if (!is_array($conditions['plugin'])) {
          $conditions['plugin'] = array($conditions['plugin']);
        }
        if (isset($conditions['plugin']) && !in_array($entity->plugin, $conditions['plugin'])) {
          unset($entities[$key]);
        }
      }
      unset($conditions['plugin']);
    }
    if (!empty($conditions['tags'])) {
      foreach ($entities as $key => $entity) {
        foreach ($conditions['tags'] as $tag) {
          if (in_array($tag, $entity->tags)) {
            continue 2;
          }
        }
        unset($entities[$key]);
      }
      unset($conditions['tags']);
    }
    return parent::applyConditions($entities, $conditions);
  }


  /**
   * Overridden to work with Rules' custom export format.
   * 
   * NOT TESTED.
   * 
   * Verbatim copy of RulesEntityController::export().
   *
   * @param string $export
   *   A serialized string in JSON format as produced by the
   *   RulesPlugin::export() method, or the PHP export as usual PHP array.
   * @param string $error_msg
   *   The error message.
   */
  public function import($export, &$error_msg = '') {
    $export = is_array($export) ? $export : backdrop_json_decode($export);
    if (!is_array($export)) {
      $error_msg = t('Unable to parse the pasted export.');
      return FALSE;
    }
    // The key is the configuration name and the value the actual export.
    $name = key($export);
    $export = current($export);
    if (!isset($export['PLUGIN'])) {
      $error_msg = t('Export misses plugin information.');
      return FALSE;
    }
    // Create an empty configuration, re-set basic keys and import.
    $config = rules_plugin_factory($export['PLUGIN']);
    $config->name = $name;
    foreach (array('label', 'active', 'weight', 'tags', 'access_exposed', 'owner') as $key) {
      if (isset($export[strtoupper($key)])) {
        $config->$key = $export[strtoupper($key)];
      }
    }
    if (!empty($export['REQUIRES'])) {
      foreach ($export['REQUIRES'] as $module) {
        if (!module_exists($module)) {
          $error_msg = t('Missing the required module %module.', array('%module' => $module));
          return FALSE;
        }
      }
      $config->dependencies = $export['REQUIRES'];
    }
    $config->import($export);
    return $config;
  }

  /**
   * Overridden.
   * 
   * Adaptation of RulesEntityController::save().
   */
  public function save($rules_config, DatabaseTransaction $transaction = NULL) {
    // At times (e.g. during import) some of the $rules_config defaults are not
    // set. When Rules are in DB, this is not a problem - Backdrop creates
    // defaults defined in hook_schema(). With CMI with need to set them
    // explicitly.
    $this->setDefaults($rules_config);

    // At this point, is_new is not set. The only way to see if this existed is
    // to try to load it.
    $original = $this->load(array($rules_config->name));
    if (!empty($original)) {
      $original = reset($original);
      $rules_config->original = $original;
    }
    else {
      $original = NULL;
      $rules_config->is_new = TRUE;
    }

    // We must run serialize, as the object has to run __sleep to select specific
    // properties to be saved.
    $rules_config->data = serialize($rules_config);
    // We are forced to still use the property id, because RulesPlugin does not
    // save 'name' when it "sleeps" the object for serialization.
    $rules_config->id = $rules_config->name;
    $return = parent::save($rules_config, $transaction);
    // Remove the old entity from the static cache.
    unset($this->entityCache[$rules_config->name]);


    // See if there are any events that have been removed.
    if ($original && $rules_config->plugin == 'reaction rule') {
      foreach (array_diff($original->events(), $rules_config->events()) as $event_name) {
        // Check if the event handler implements the event dispatcher interface.
        $handler = rules_get_event_handler($event_name, $rules_config->getEventSettings($event_name));
        if (!$handler instanceof RulesEventDispatcherInterface) {
          continue;
        }

        // Only stop an event dispatcher if there are no rules for it left.
        if (!rules_config_load_multiple(FALSE, array('event' => $event_name, 'plugin' => 'reaction rule', 'active' => TRUE)) && $handler->isWatching()) {
          $handler->stopWatching();
        }
      }
    }

    return $return;
  }

  /**
   * Overridden.
   * 
   * Adaptation of Adaptation of RulesEntityController::delete()
   */
  public function delete($ids, DatabaseTransaction $transaction = NULL) {
    $configs = $ids ? entity_load('rules_config', $ids) : array();
    $return = parent::delete($ids, $transaction);
    // Stop event dispatchers when deleting the last rule of an event set.
    $processed = array();
    foreach ($configs as $config) {
      if ($config->getPluginName() != 'reaction rule') {
        continue;
      }

      foreach ($config->events() as $event_name) {
        // Only process each event once.
        if (!empty($processed[$event_name])) {
          continue;
        }
        $processed[$event_name] = TRUE;

        // Check if the event handler implements the event dispatcher interface.
        $handler = rules_get_event_handler($event_name, $config->getEventSettings($event_name));
        if (!$handler instanceof RulesEventDispatcherInterface) {
          continue;
        }

        // Only stop an event dispatcher if there are no rules for it left.
        if ($handler->isWatching() && !rules_config_load_multiple(FALSE, array('event' => $event_name, 'plugin' => 'reaction rule', 'active' => TRUE))) {
          $handler->stopWatching();
        }
      }
    }
    return $return;
  }

  /**
   * Sets some default values for $rules_config.
   */
  protected function setDefaults($rules_config) {
    $defaults = array(
      'label' => 'unlabeled',
      'active' => 1,
      'weight' => 0,
      'dirty' => 0,
      'owner' => 'rules',
      'access_exposed' => 0,
      'status' => ENTITY_PLUS_CUSTOM,
      'tags' => array(),
    );
    foreach ($defaults as $key => $value) {
      if (!isset($rules_config->$key)) {
        $rules_config->$key = $value;
      }
    }
  }
}
