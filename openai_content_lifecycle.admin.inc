<?php

/**
 * @file
 * Administrative forms for OpenAI Content Lifecycle module.
 */

/**
 * Settings form for OpenAI Content Lifecycle.
 */
function openai_content_lifecycle_settings_form($form, &$form_state) {
  $config = config('openai_content_lifecycle.settings');

  $form['#config'] = 'openai_content_lifecycle.settings';

  // Get example text.
  $example = t("- it mentions the queen of england.\n- it mentions the king of Germany.\n- it contains inconsistencies or contradictions\n- you are really sure that it is outdated and a human should check it");

  // Main prompt field - most important, so show it first.
  $form['default_prompt'] = array(
    '#type' => 'textarea',
    '#title' => t('Mark my content when'),
    '#default_value' => $config->get('default_prompt') ?: _openai_content_lifecycle_default_prompt(),
    '#description' => t('Type something like "it mentions the old tax rate of 19% which is now 21%."'),
    '#rows' => 4,
    '#placeholder' => $example,
  );

  // AI settings - advanced, so collapse by default.
  $form['ai_settings'] = array(
    '#type' => 'fieldset',
    '#title' => t('Advanced LLM settings'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#tree' => TRUE,
    '#weight' => 4,
  );

  // Get available OpenAI models.
  $model_options = array();
  if (module_exists('openai')) {
    try {
      $api_key_config = config_get('openai.settings', 'api_key');
      if (module_exists('key') && !empty($api_key_config)) {
        $api_key = key_get_key_value($api_key_config);
        if (!empty($api_key)) {
          $openai_api = new OpenAIApi($api_key);
          $models = $openai_api->filterModels(array('gpt', 'o1', 'o3'));
          foreach ($models as $model_id => $model_name) {
            $model_options[$model_id] = $model_name;
          }
        }
      }
    }
    catch (Exception $e) {
      watchdog('openai_content_lifecycle', 'Error loading OpenAI models: @message', array('@message' => $e->getMessage()), WATCHDOG_ERROR);
    }
  }

  // Fallback model options if we can't get them from API.
  if (empty($model_options)) {
    $model_options = array(
      'gpt-4o-mini' => 'GPT-4o Mini',
      'gpt-4o' => 'GPT-4o',
      'gpt-4-turbo' => 'GPT-4 Turbo',
      'gpt-3.5-turbo' => 'GPT-3.5 Turbo',
    );
  }

  $form['ai_settings']['default_model'] = array(
    '#type' => 'select',
    '#title' => t('LLM to use for content evaluation'),
    '#description' => t('Select which OpenAI model to use for content analysis.'),
    '#options' => $model_options,
    '#default_value' => $config->get('default_model') ?: 'gpt-4o-mini',
    '#required' => TRUE,
  );

  $form['ai_settings']['temperature'] = array(
    '#type' => 'textfield',
    '#title' => t('Temperature'),
    '#description' => t('Controls randomness in responses. Lower values (0.1-0.5) make output more focused and deterministic. Default is 0.7.'),
    '#default_value' => $config->get('temperature') ?: 0.7,
    '#size' => 10,
    '#element_validate' => array('element_validate_number'),
  );

  $form['ai_settings']['pre_prompt'] = array(
    '#type' => 'textarea',
    '#title' => t('Pre prompt'),
    '#description' => t('The prompt used for marking content. You can use the following tokens:<br><strong>[conditions]</strong> (the reasons mentioned above why content might be requiring an update)<br><strong>[ai_content_lifecycle:date]</strong> The current date<br><strong>[lang]</strong> The current language of the admin interface'),
    '#default_value' => $config->get('pre_prompt') ?: _openai_content_lifecycle_default_system_prompt(),
    '#rows' => 25,
  );

  // Description for content types section.
  $form['description'] = array(
    '#markup' => '<p>' . t('Select which content entities should be checked by the AI Content Lifecycle system.') . '</p>',
  );

  // Content type selection.
  $form['content_types'] = array(
    '#type' => 'fieldset',
    '#title' => t('What to check'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    '#tree' => TRUE,
  );

  $enabled_types = $config->get('enabled_types') ?: array();
  $bundle_prompts = $config->get('bundle_prompts') ?: array();

  // Get all node types.
  $node_types = node_type_get_types();
  
  foreach ($node_types as $type => $type_info) {
    $form['content_types'][$type] = array(
      '#type' => 'fieldset',
      '#title' => check_plain($type_info->name),
      '#collapsible' => TRUE,
      '#collapsed' => empty($enabled_types[$type]),
    );

    $form['content_types'][$type]['enabled'] = array(
      '#type' => 'checkbox',
      '#title' => t('Analyze @type content', array('@type' => $type_info->name)),
      '#default_value' => !empty($enabled_types[$type]),
    );

    $form['content_types'][$type]['prompt'] = array(
      '#type' => 'textarea',
      '#title' => t('Mark @type when', array('@type' => $type_info->name)),
      '#description' => t('Override default conditions for this content type. Leave empty to use the default conditions above.'),
      '#default_value' => isset($bundle_prompts['node'][$type]) ? $bundle_prompts['node'][$type] : '',
      '#rows' => 6,
      '#states' => array(
        'visible' => array(
          ':input[name="content_types[' . $type . '][enabled]"]' => array('checked' => TRUE),
        ),
      ),
    );
  }

  // Batch processing.
  $form['batch'] = array(
    '#type' => 'fieldset',
    '#title' => t('Batch Processing'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );

  $form['batch']['info'] = array(
    '#markup' => '<p>' . t('After saving your settings, you can use the button below to analyze all existing content matching your criteria.') . '</p>',
  );

  $form['batch']['run_batch'] = array(
    '#type' => 'submit',
    '#value' => t('Analyze Existing Content'),
    '#submit' => array('openai_content_lifecycle_settings_form_batch_submit'),
  );

  $form['actions']['#type'] = 'actions';
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  );

  return $form;
}

/**
 * Validation handler for settings form.
 */
function openai_content_lifecycle_settings_form_validate($form, &$form_state) {
  // Validate temperature value.
  $temperature = $form_state['values']['ai_settings']['temperature'];
  
  // Convert to float for validation
  $temp_float = floatval($temperature);
  
  if ($temp_float < 0 || $temp_float > 2) {
    form_set_error('ai_settings][temperature', t('Temperature must be a number between 0 and 2.'));
  }
}

/**
 * Submit handler for settings form.
 */
function openai_content_lifecycle_settings_form_submit($form, &$form_state) {
  $config = config('openai_content_lifecycle.settings');

  // Save AI settings (convert temperature to float).
  $config->set('default_model', $form_state['values']['ai_settings']['default_model']);
  $config->set('temperature', floatval($form_state['values']['ai_settings']['temperature']));
  $config->set('pre_prompt', $form_state['values']['ai_settings']['pre_prompt']);
  $config->set('default_prompt', $form_state['values']['default_prompt']);

  // Save enabled content types and prompts.
  $enabled_types = array();
  $bundle_prompts = array();

  foreach ($form_state['values']['content_types'] as $type => $values) {
    if (!empty($values['enabled'])) {
      $enabled_types[$type] = TRUE;
      
      if (!empty($values['prompt'])) {
        $bundle_prompts['node'][$type] = $values['prompt'];
      }
    }
  }

  $config->set('enabled_types', $enabled_types);
  $config->set('bundle_prompts', $bundle_prompts);
  $config->save();

  backdrop_set_message(t('OpenAI Content Lifecycle settings have been saved.'));
}

/**
 * Batch submit handler - redirect to batch confirmation page.
 */
function openai_content_lifecycle_settings_form_batch_submit($form, &$form_state) {
  // First save the form.
  openai_content_lifecycle_settings_form_submit($form, $form_state);
  
  // Then redirect to batch form.
  $form_state['redirect'] = 'admin/config/content/openai-content-lifecycle/batch';
}

/**
 * Batch processing form.
 */
function openai_content_lifecycle_batch_form($form, &$form_state) {
  $config = config('openai_content_lifecycle.settings');
  $enabled_types = $config->get('enabled_types') ?: array();

  if (empty($enabled_types)) {
    $form['error'] = array(
      '#markup' => '<p>' . t('No content types are enabled for analysis. Please configure the module settings first.') . '</p>',
    );
    return $form;
  }

  $form['info'] = array(
    '#markup' => '<p>' . t('This will analyze all published content of the selected types. The process may take some time depending on how much content you have.') . '</p>',
  );

  $form['confirm'] = array(
    '#type' => 'checkbox',
    '#title' => t('I understand this may take some time and consume OpenAI API credits.'),
    '#required' => TRUE,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Start Analysis'),
  );

  $form['actions']['cancel'] = array(
    '#type' => 'link',
    '#title' => t('Cancel'),
    '#href' => 'admin/config/content/openai-content-lifecycle',
  );

  return $form;
}

/**
 * Submit handler for batch form.
 */
function openai_content_lifecycle_batch_form_submit($form, &$form_state) {
  $config = config('openai_content_lifecycle.settings');
  $enabled_types = $config->get('enabled_types') ?: array();

  // Collect all nodes to analyze.
  $nids = array();
  foreach (array_keys($enabled_types) as $type) {
    $query = new EntityFieldQuery();
    $query->entityCondition('entity_type', 'node')
      ->entityCondition('bundle', $type)
      ->propertyCondition('status', 1); // Only published nodes
    
    $result = $query->execute();
    
    if (isset($result['node'])) {
      $nids = array_merge($nids, array_keys($result['node']));
    }
  }

  if (empty($nids)) {
    backdrop_set_message(t('No content found to analyze.'), 'warning');
    $form_state['redirect'] = 'admin/config/content/openai-content-lifecycle';
    return;
  }

  // Set up batch.
  $batch = array(
    'operations' => array(),
    'finished' => 'openai_content_lifecycle_batch_finished',
    'title' => t('Analyzing Content'),
    'init_message' => t('Starting content analysis...'),
    'progress_message' => t('Processed @current out of @total.'),
    'error_message' => t('Content analysis has encountered an error.'),
    'file' => backdrop_get_path('module', 'openai_content_lifecycle') . '/openai_content_lifecycle.admin.inc',
  );

  // Add operations in chunks.
  $chunk_size = 10;
  $chunks = array_chunk($nids, $chunk_size);
  
  foreach ($chunks as $chunk) {
    $batch['operations'][] = array(
      'openai_content_lifecycle_batch_process',
      array($chunk),
    );
  }

  batch_set($batch);
  $form_state['redirect'] = 'admin/config/content/openai-content-lifecycle';
}

/**
 * Batch operation callback.
 */
function openai_content_lifecycle_batch_process($nids, &$context) {
  if (!isset($context['results']['processed'])) {
    $context['results']['processed'] = 0;
    $context['results']['flagged'] = 0;
    $context['results']['errors'] = 0;
  }

  $analyzer = new OpenAIContentLifecycleAnalyzer();

  foreach ($nids as $nid) {
    $node = node_load($nid);
    
    if ($node) {
      try {
        $analysis = $analyzer->analyzeContent($node);
        
        if (!empty($analysis)) {
          // Store analysis result.
          _openai_content_lifecycle_store_analysis($node, $analysis);
          $context['results']['flagged']++;
          
          $context['message'] = t('Flagged: @title', array('@title' => $node->title));
        }
        else {
          $context['message'] = t('Checked: @title (OK)', array('@title' => $node->title));
        }
        
        $context['results']['processed']++;
      }
      catch (Exception $e) {
        watchdog('openai_content_lifecycle', 'Error analyzing node @nid: @message', array('@nid' => $nid, '@message' => $e->getMessage()), WATCHDOG_ERROR);
        $context['results']['errors']++;
      }
    }
    
    // Small delay to avoid rate limiting.
    usleep(100000); // 0.1 seconds
  }
}

/**
 * Batch finished callback.
 */
function openai_content_lifecycle_batch_finished($success, $results, $operations) {
  if ($success) {
    $message = format_plural(
      $results['processed'],
      'Analyzed 1 content item.',
      'Analyzed @count content items.'
    );
    $message .= ' ' . format_plural(
      $results['flagged'],
      '1 item was flagged for review.',
      '@count items were flagged for review.'
    );
    
    if (!empty($results['errors'])) {
      $message .= ' ' . format_plural(
        $results['errors'],
        '1 error occurred.',
        '@count errors occurred.'
      );
    }
    
    backdrop_set_message($message);
  }
  else {
    backdrop_set_message(t('An error occurred during processing.'), 'error');
  }
}

/**
 * Helper function to store analysis results.
 */
function _openai_content_lifecycle_store_analysis($node, $analysis) {
  // Create or update lifecycle entity.
  $analyzer = new OpenAIContentLifecycleAnalyzer();
  $lifecycle = $analyzer->updateContentLifecycleWithAnalysis($node, NULL);
  
  if ($lifecycle) {
    // Also set a flag on the node if the flag module is available.
    if (module_exists('flag')) {
      $flag = flag_get_flag('needs_review');
      if ($flag) {
        $flag->flag('flag', $node->nid);
      }
    }
  }
}
