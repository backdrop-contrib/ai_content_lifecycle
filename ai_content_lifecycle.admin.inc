<?php


/**
 * @file
 * Administrative forms for AI Content Lifecycle module.
 */

/**
 * Settings form for AI Content Lifecycle.
 */
function ai_content_lifecycle_settings_form($form, &$form_state) {
  $config = config('ai_content_lifecycle.settings');

  $form['#config'] = 'ai_content_lifecycle.settings';

  // Get example text.
  $example = t("- it mentions the queen of england.\n- it mentions the king of Germany.\n- it contains inconsistencies or contradictions\n- you are really sure that it is outdated and a human should check it");

  // Main prompt field - most important, so show it first.
  $form['default_prompt'] = [
    '#type' => 'textarea',
    '#title' => t('Mark my content when'),
    '#default_value' => $config->get('default_prompt') ?: _ai_content_lifecycle_default_prompt(),
    '#description' => t('Type something like "it mentions the old tax rate of 19% which is now 21%."'),
    '#rows' => 4,
    '#placeholder' => $example,
  ];

  // AI settings - advanced, so collapse by default.
  $form['ai_settings'] = [
    '#type' => 'fieldset',
    '#title' => t('Advanced LLM settings'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#tree' => TRUE,
    '#weight' => 4,
  ];

  // Similarity search configuration (Search API).
  $form['similarity'] = [
    '#type' => 'fieldset',
    '#title' => t('Similarity search'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    '#tree' => TRUE,
    '#weight' => 5,
  ];

  $similarity_mode_default = $config->get('similarity_mode');
  if (empty($similarity_mode_default)) {
    $similarity_mode_default = !empty($config->get('similarity_index')) ? 'search_api' : 'none';
  }

  $form['similarity']['similarity_mode'] = [
    '#type' => 'select',
    '#title' => t('Similarity mode'),
    '#options' => [
      'none' => t('None'),
      'search_api' => t('Search API (fulltext]'),
      'vector' => t('Vector search'),
    ],
    '#default_value' => $similarity_mode_default,
    '#description' => t('Choose how to find potentially similar/duplicate content.'),
  ];

  if (module_exists('search_api')) {
    $index_options = [];
    foreach (entity_load_multiple('search_api_index') as $index) {
      $index_options[$index->machine_name] = $index->name;
    }

    $form['similarity']['similarity_index'] = [
      '#type' => 'select',
      '#title' => t('Search API index for duplicate detection'),
      '#description' => t('Select a Search API index to retrieve potentially similar content to include in the AI prompt. Leave empty to skip similarity lookup.'),
      '#options' => ['' => t('- None -')] + $index_options,
      '#default_value' => $config->get('similarity_index') ?: '',
      '#disabled' => empty($index_options),
      '#states' => [
        'visible' => [
          ':input[name="similarity[similarity_mode]"]' => ['value' => 'search_api'],
        ],
      ],
    ];

    if (empty($index_options)) {
      $form['similarity']['help'] = [
        '#markup' => '<p>' . t('No Search API indexes found. Create an index to enable similarity lookups.') . '</p>',
      ];
    }
  }
  else {
    $form['similarity']['notice'] = [
      '#markup' => '<p>' . t('Enable the Search API module and create an index to use similarity context in prompts.') . '</p>',
    ];
  }

  // Vector search settings.
  $form['similarity']['vector'] = [
    '#type' => 'fieldset',
    '#title' => t('Vector search settings'),
    '#tree' => TRUE,
    '#states' => [
      'visible' => [
        ':input[name="similarity[similarity_mode]"]' => ['value' => 'vector'],
      ],
    ],
  ];

  $vector_index_options = [];
  if (module_exists('search_api')) {
    foreach (entity_load_multiple('search_api_index') as $index) {
      $vector_index_options[$index->machine_name] = $index->name;
    }
  }

  $form['similarity']['vector']['vector_index_id'] = [
    '#type' => 'select',
    '#title' => t('Vector Search API index'),
    '#description' => t('Select the ai_search-backed Search API index to use for vector similarity search.'),
    '#options' => !empty($vector_index_options) ? $vector_index_options : ['' => t('- No Search API indexes available -')],
    '#default_value' => $config->get('vector_index_id') ?: '',
    '#required' => TRUE,
    '#disabled' => empty($vector_index_options),
  ];

  $form['similarity']['vector']['vector_top_k'] = [
    '#type' => 'number',
    '#title' => t('Top K matches'),
    '#description' => t('How many vector matches to retrieve for duplicate detection.'),
    '#default_value' => $config->get('vector_top_k') ?: 5,
    '#min' => 1,
    '#max' => 50,
  ];

  $form['similarity']['vector']['vector_score_threshold'] = [
    '#type' => 'textfield',
    '#title' => t('Similarity score threshold'),
    '#description' => t('Only include vector matches at or above this score (leave empty for no threshold].'),
    '#default_value' => $config->get('vector_score_threshold') !== NULL ? $config->get('vector_score_threshold') : '',
    '#size' => 10,
  ];

  $form['similarity']['vector']['reindex_note'] = [
    '#markup' => '<p>' . t('To build or refresh embeddings, <a href="@url">reindex via Search API</a>. The ai_search module handles all embedding generation during indexing.', [
      '@url' => url('admin/structure/search'),
    ]) . '</p>',
    '#states' => [
      'visible' => [
        ':input[name="similarity[similarity_mode]"]' => ['value' => 'vector'],
      ],
    ],
  ];

  // Model selector: use ai module helper for all chat models from all providers.
  $model_options = ai_get_all_chat_models() ?: [];

  $form['ai_settings']['default_model'] = [
    '#type' => 'select',
    '#title' => t('LLM to use for content evaluation'),
    '#description' => t('Select which AI model to use for content analysis.'),
    '#options' => !empty($model_options) ? $model_options : ['' => t('- No chat models available -')],
    '#default_value' => $config->get('default_model') ?: 'gpt-4o-mini',
    '#required' => TRUE,
    '#disabled' => empty($model_options),
  ];

  $form['ai_settings']['temperature'] = [
    '#type' => 'textfield',
    '#title' => t('Temperature'),
    '#description' => t('Controls randomness in responses. Lower values (0.1-0.5] make output more focused and deterministic. Default is 0.7.'),
    '#default_value' => $config->get('temperature') ?: 0.7,
    '#size' => 10,
    '#element_validate' => ['element_validate_number'],
  ];

  $form['ai_settings']['pre_prompt'] = [
    '#type' => 'textarea',
    '#title' => t('Pre prompt'),
    '#description' => t('The prompt used for marking content. You can use the following tokens:<br><strong>[conditions]</strong> (the reasons mentioned above why content might be requiring an update]<br><strong>[ai_content_lifecycle:date]</strong> The current date<br><strong>[lang]</strong> The current language of the admin interface'),
    '#default_value' => $config->get('pre_prompt') ?: _ai_content_lifecycle_default_system_prompt(),
    '#rows' => 25,
  ];

  // Background processing settings.
  $form['background'] = [
    '#type' => 'fieldset',
    '#title' => t('Background processing'),
    '#weight' => 3,
  ];
  $queue = BackdropQueue::get('ai_content_lifecycle_analysis');
  $queue_depth = $queue->numberOfItems();
  $form['background']['queue_info'] = [
    '#markup' => '<p>' . t('Analysis queue: <strong>@count</strong> node(s) pending.', ['@count' => $queue_depth]) . '</p>',
  ];
  $form['background']['auto_upsert_on_save'] = [
    '#type' => 'checkbox',
    '#title' => t('Queue nodes for re-analysis on save'),
    '#description' => t('Automatically queue nodes for analysis whenever they are created or updated. Leave off to only process content queued manually.'),
    '#default_value' => $config->get('auto_upsert_on_save') ?: FALSE,
  ];
  $form['background']['queue_all'] = [
    '#type' => 'submit',
    '#value' => t('Queue All Existing Content'),
    '#submit' => ['ai_content_lifecycle_settings_form_queue_all_submit'],
    '#limit_validation_errors' => [],
  ];
  $form['background']['cron_batch_size'] = [
    '#type' => 'select',
    '#title' => t('Nodes to analyze per cron run'),
    '#options' => [5 => 5, 10 => 10, 25 => 25, 50 => 50],
    '#default_value' => $config->get('cron_batch_size') ?: 10,
    '#description' => t('Queues all published nodes of enabled types. Run cron to process them gradually.'),
  ];

  // Description for content types section.
  $form['description'] = [
    '#markup' => '<p>' . t('Select which content entities should be checked by the AI Content Lifecycle system.') . '</p>',
  ];

  // Content type selection.
  $form['content_types'] = [
    '#type' => 'fieldset',
    '#title' => t('What to check'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    '#tree' => TRUE,
  ];

  $enabled_types = $config->get('enabled_types') ?: [];
  $bundle_prompts = $config->get('bundle_prompts') ?: [];

  // Get all node types.
  $node_types = node_type_get_types();

  foreach ($node_types as $type => $type_info) {
    $form['content_types'][$type] = [
      '#type' => 'fieldset',
      '#title' => check_plain($type_info->name),
      '#collapsible' => TRUE,
      '#collapsed' => empty($enabled_types[$type]),
    ];

    $form['content_types'][$type]['enabled'] = [
      '#type' => 'checkbox',
      '#title' => t('Analyze @type content', ['@type' => $type_info->name]),
      '#default_value' => !empty($enabled_types[$type]),
    ];

    $form['content_types'][$type]['prompt'] = [
      '#type' => 'textarea',
      '#title' => t('Mark @type when', ['@type' => $type_info->name]),
      '#description' => t('Override default conditions for this content type. Leave empty to use the default conditions above.'),
      '#default_value' => isset($bundle_prompts['node'][$type]) ? $bundle_prompts['node'][$type] : '',
      '#rows' => 6,
      '#states' => [
        'visible' => [
          ':input[name="content_types[' . $type . '][enabled]"]' => ['checked' => TRUE],
        ],
      ],
    ];
  }

  // Batch processing.
  $form['batch'] = [
    '#type' => 'fieldset',
    '#title' => t('Batch Processing'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  ];

  $form['batch']['info'] = [
    '#markup' => '<p>' . t('Analyze all existing content immediately in one request. May time out on large sites — use "Queue All Existing Content" above instead.') . '</p>',
  ];

  $form['batch']['run_batch'] = [
    '#type' => 'submit',
    '#value' => t('Analyze Existing Content Now'),
    '#submit' => ['ai_content_lifecycle_settings_form_batch_submit'],
  ];

  $form['actions']['#type'] = 'actions';
  $form['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  ];

  return $form;
}

/**
 * Validation handler for settings form.
 */
function ai_content_lifecycle_settings_form_validate($form, &$form_state) {
  // Validate temperature value.
  $temperature = $form_state['values']['ai_settings']['temperature'];

  // Convert to float for validation
  $temp_float = floatval($temperature);

  if ($temp_float < 0 || $temp_float > 2) {
    form_set_error('ai_settings][temperature', t('Temperature must be a number between 0 and 2.'));
  }

  $similarity = $form_state['values']['similarity'];
  $mode = !empty($similarity['similarity_mode']) ? $similarity['similarity_mode'] : 'none';

  // Validate similarity index selection when Search API is available.
  if ($mode === 'search_api' && !empty($similarity['similarity_index'])) {
    if (!module_exists('search_api')) {
      form_set_error('similarity][similarity_index', t('Search API must be enabled to select an index.'));
    }
    else {
      $index_id = $similarity['similarity_index'];
      $exists = search_api_index_load($index_id);
      if (!$exists) {
        form_set_error('similarity][similarity_index', t('The selected Search API index was not found.'));
      }
    }
  }

  // Validate vector settings when enabled.
  if ($mode === 'vector') {
    if (!module_exists('ai_search')) {
      form_set_error('similarity][vector][vector_index_id', t('The ai_search module must be enabled for vector mode.'));
    }
    elseif (!module_exists('search_api')) {
      form_set_error('similarity][vector][vector_index_id', t('Search API must be enabled for vector mode.'));
    }
    else {
      $vector_index = $similarity['vector']['vector_index_id'];
      $exists = !empty($vector_index) ? search_api_index_load($vector_index) : FALSE;
      if (!$exists) {
        form_set_error('similarity][vector][vector_index_id', t('The selected vector index was not found.'));
      }
    }

    if (!empty($similarity['vector']['vector_top_k']) && intval($similarity['vector']['vector_top_k']) < 1) {
      form_set_error('similarity][vector][vector_top_k', t('Top K must be at least 1.'));
    }

    if ($similarity['vector']['vector_score_threshold'] !== '' && !is_numeric($similarity['vector']['vector_score_threshold'])) {
      form_set_error('similarity][vector][vector_score_threshold', t('Similarity score threshold must be a number.'));
    }
  }
}

/**
 * Submit handler for settings form.
 */
function ai_content_lifecycle_settings_form_submit($form, &$form_state) {
  $config = config('ai_content_lifecycle.settings');

  // Save AI settings (convert temperature to float).
  $config->set('default_model', $form_state['values']['ai_settings']['default_model']);
  $config->set('temperature', floatval($form_state['values']['ai_settings']['temperature']));
  $config->set('pre_prompt', $form_state['values']['ai_settings']['pre_prompt']);
  $config->set('cron_batch_size', (int) $form_state['values']['cron_batch_size']);
  $config->set('auto_upsert_on_save', !empty($form_state['values']['auto_upsert_on_save']));
  $config->set('default_prompt', $form_state['values']['default_prompt']);
  $similarity = $form_state['values']['similarity'];
  $mode = !empty($similarity['similarity_mode']) ? $similarity['similarity_mode'] : 'none';

  $config->set('similarity_mode', $mode);
  $config->set('similarity_index', ($mode === 'search_api' && !empty($similarity['similarity_index'])) ? $similarity['similarity_index'] : '');
  $config->set('vector_index_id', !empty($similarity['vector']['vector_index_id']) ? $similarity['vector']['vector_index_id'] : '');
  $config->set('vector_top_k', isset($similarity['vector']['vector_top_k']) ? intval($similarity['vector']['vector_top_k']) : 5);
  $config->set('vector_score_threshold', $similarity['vector']['vector_score_threshold'] !== '' ? floatval($similarity['vector']['vector_score_threshold']) : '');

  // Save enabled content types and prompts.
  $enabled_types = [];
  $bundle_prompts = [];

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

  backdrop_set_message(t('AI Content Lifecycle settings have been saved.'));
}

/**
 * Submit handler to queue all existing content for background analysis.
 */
function ai_content_lifecycle_settings_form_queue_all_submit($form, &$form_state) {
  $config = config('ai_content_lifecycle.settings');
  $enabled_types = $config->get('enabled_types') ?: [];

  if (empty($enabled_types)) {
    backdrop_set_message(t('No content types are enabled for analysis.'), 'warning');
    return;
  }

  $queue = BackdropQueue::get('ai_content_lifecycle_analysis');
  $queued = 0;

  foreach (array_keys($enabled_types) as $type) {
    $query = new EntityFieldQuery();
    $query->entityCondition('entity_type', 'node')
      ->entityCondition('bundle', $type)
      ->propertyCondition('status', 1);
    $result = $query->execute();
    if (!empty($result['node'])) {
      foreach (array_keys($result['node']) as $nid) {
        $queue->createItem(['nid' => (int) $nid]);
        $queued++;
      }
    }
  }

  backdrop_set_message(t('Queued @count node(s) for analysis. Run cron to process them.', ['@count' => $queued]));
}

/**
 * Batch submit handler - redirect to batch confirmation page.
 */
function ai_content_lifecycle_settings_form_batch_submit($form, &$form_state) {
  // First save the form.
  ai_content_lifecycle_settings_form_submit($form, $form_state);

  // Then redirect to batch form.
  $form_state['redirect'] = 'admin/config/ai/ai-content-lifecycle/batch';
}

/**
 * Batch processing form.
 */
function ai_content_lifecycle_batch_form($form, &$form_state) {
  $config = config('ai_content_lifecycle.settings');
  $enabled_types = $config->get('enabled_types') ?: [];

  if (empty($enabled_types)) {
    $form['error'] = [
      '#markup' => '<p>' . t('No content types are enabled for analysis. Please configure the module settings first.') . '</p>',
    ];
    return $form;
  }

  $form['info'] = [
    '#markup' => '<p>' . t('This will analyze all published content of the selected types. The process may take some time depending on how much content you have.') . '</p>',
  ];

  $form['confirm'] = [
    '#type' => 'checkbox',
    '#title' => t('I understand this may take some time and consume AI provider credits.'),
    '#required' => TRUE,
  ];

  $form['actions'] = ['#type' => 'actions'];
  $form['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Start Analysis'),
  ];

  $form['actions']['cancel'] = [
    '#type' => 'link',
    '#title' => t('Cancel'),
    '#href' => 'admin/config/ai/ai-content-lifecycle',
  ];

  return $form;
}

/**
 * Submit handler for batch form.
 */
function ai_content_lifecycle_batch_form_submit($form, &$form_state) {
  $config = config('ai_content_lifecycle.settings');
  $enabled_types = $config->get('enabled_types') ?: [];

  // Collect all nodes to analyze.
  $nids = [];
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
    $form_state['redirect'] = 'admin/config/ai/ai-content-lifecycle';
    return;
  }

  // Set up batch.
  $batch = [
    'operations' => [],
    'finished' => 'ai_content_lifecycle_batch_finished',
    'title' => t('Analyzing Content'),
    'init_message' => t('Starting content analysis...'),
    'progress_message' => t('Processed @current out of @total.'),
    'error_message' => t('Content analysis has encountered an error.'),
    'file' => backdrop_get_path('module', 'ai_content_lifecycle') . '/ai_content_lifecycle.admin.inc',
  ];

  // Add operations in chunks.
  $chunk_size = 10;
  $chunks = array_chunk($nids, $chunk_size);

  foreach ($chunks as $chunk) {
    $batch['operations'][] = [
      'ai_content_lifecycle_batch_process',
      [$chunk],
    ];
  }

  batch_set($batch);
  $form_state['redirect'] = 'admin/config/ai/ai-content-lifecycle';
}

/**
 * Batch operation callback.
 */
function ai_content_lifecycle_batch_process($nids, &$context) {
  if (!isset($context['results']['processed'])) {
    $context['results']['processed'] = 0;
    $context['results']['flagged'] = 0;
    $context['results']['errors'] = 0;
  }

  $analyzer = new AIContentLifecycleAnalyzer();

  foreach ($nids as $nid) {
    $node = node_load($nid);

    if (!$node) {
      continue;
    }

    try {
      $lifecycle = $analyzer->updateContentLifecycleWithAnalysis($node, NULL);

      if ($lifecycle && $lifecycle->review_status === 'analyzed') {
        $context['results']['flagged']++;
        $context['message'] = t('Flagged: @title', ['@title' => $node->title]);
      }
      else {
        $context['message'] = t('OK: @title', ['@title' => $node->title]);
      }

      $context['results']['processed']++;
    }
    catch (\Exception $e) {
      watchdog('ai_content_lifecycle', 'Error analyzing node @nid: @message', ['@nid' => $nid, '@message' => $e->getMessage()], WATCHDOG_ERROR);
      $context['results']['errors']++;
    }

    usleep(100000);
  }
}

/**
 * Batch finished callback.
 */
function ai_content_lifecycle_batch_finished($success, $results, $operations) {
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

