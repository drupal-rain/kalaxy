<?php
/**
 * @file
 * autocomplete_callback.inc.
 *
 * Autocomplete callback for 'entityreference' Form API Elements.
 */

/**
 * Page callback for the entityreference elements autocomplete path.
 */
function entityreference_autocomplete_autocomplete_callback($entity_type, $bundles, $encoded_query_settings, $string = '') {
  $matches = array();

  // Get the query settings as if they were a query array, since they're built
  // with http_build_query().
  $query_settings = drupal_get_query_array(urldecode($encoded_query_settings));

  // The user enters a comma-separated list of entity labels. Autocomplete only
  // the last label.
  $labels_typed = entityreference_autocomplete_explode_tags($string);
  $last_label = drupal_strtolower(array_pop($labels_typed));
  if (!empty($last_label)) {
    $prefix = count($labels_typed) ? implode(', ', $labels_typed) . ', ' : '';
  }

  if (isset($last_label)) {
    // Get entity metadata, to be used for some checks.
    $entity_info = entity_get_info($entity_type);

    $query = new EntityFieldQuery();
    $query->entityCondition('entity_type', $entity_type);

    // If $bundless is not null, add the 'bundle' condition. Also, save some
    // pain to anyone trying to specify a bundle when the entity has no actual
    // key for bundles.
    if ($bundles  && !empty($entity_info['entity keys']['bundle'])) {
      $query->entityCondition('bundle', $bundles);
    }

    // Need to fetch the column used as entity label.
    $label_column = FALSE;
    // Check if the entity has a label column defined.
    if (isset($entity_info['entity keys']['label'])) {
      $label_column = $entity_info['entity keys']['label'];
    }
    // Interestingly that Drupal's core doesn't define any label for users, is
    // that considered a core bug? TODO: Move this to a more proper place.
    else {
      switch ($entity_type) {
        case 'user':
          $label_column = 'name';
          break;
      }
    }
    // Still no label available, fall back to entity id column.
    if (!$label_column){
      $label_column = $entity_info['entity keys']['id'];
    }

    $query->propertyCondition($label_column, '%' . $last_label . '%', 'LIKE');
    // Set the maximum number of results returned.
    if (!empty($query_settings['limit']) && is_numeric($query_settings['limit'])) {
      $query->range(0, $query_settings['limit']);
    }
    $result = $query->execute();

    if (!empty($result[$entity_type])) {
      $entities = entity_load($entity_type, array_keys($result[$entity_type]));
      // $context will hold the metadata of entities found, so that modules
      // can alter the results in any way they want.
      $context = array();

      // Iterate through all entities retrieved and process the data to return
      // it as expected by Drupal javascript.
      foreach ($entities as $entity_id => $entity) {
        if (entity_access('view', $entity_type, $entity)) {
          // Get the labels for the key and for the option.
          $option = entityreference_autocomplete_label_for_reference($entity_type, $entity_id, FALSE);
          $key = entityreference_autocomplete_label_for_reference($entity_type, $entity_id);

          // $prefix . $key is the value that will be set in the textfield in
          // the browser, whereas $option is the html that is shown to the user
          // *before* he clicks in one of the options.
          $matches[$prefix . $key] = check_plain($option);

          // Store metadata about the entity returned in a variable.
          list(, , $bundle) = entity_extract_ids($entity_type, $entity);
          $context[$prefix . $key] = array(
            'entity_id' => $entity_id,
            'entity_type' => $entity_type,
            'entity_bundle' => $bundle,
            'rendered_html' => check_plain($option),
          );
        }
      }
    }
  }

  // Let other drupal modules alter the results.
  drupal_alter('entityreference_autocomplete_matches', $matches, $context);
  // Finally, output matches in json, as they're are expected by Drupal's ajax.
  drupal_json_output($matches);
}

/**
 * Explodes a string of tags into an array.
 *
 * This is almost a clone of drupal's drupal_explode_tags() function. The reason
 * to use this custom function is because drupal_explode_tags() assumes that the
 * tags entered will be saved straight into the database after being returned,
 * so it removes any escape formatting of the tags. That will make the
 * autocomplete callback to remove any quotes added to tags when users add a
 * different tag, making the escaping of previous ones to disappear, which
 * ultimately will result in an invalid value error in the validation callback.
 *
 * This function does the same as drupal_explode_tags() in terms of exploding
 * the tags, but it doesn't remove any escape formatting from the tags to be
 * returned.
 *
 * @see drupal_explode_tags()
 */
function entityreference_autocomplete_explode_tags($tags) {
  $regexp = '%(?:^|,\ *)("(?>[^"]*)(?>""[^"]* )*"|(?: [^",]*))%x';
  preg_match_all($regexp, $tags, $matches);
  $typed_tags = array_unique($matches[1]);
  return $typed_tags;
}
