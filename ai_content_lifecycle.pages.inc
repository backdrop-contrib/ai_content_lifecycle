<?php

/**
 * @file
 * Page callbacks for AI Content Lifecycle module.
 */

/**
 * Page callback: List all content lifecycle entities.
 */
function ai_content_lifecycle_list_page() {
  $header = [
    ['data' => t('Content'), 'field' => 'label'],
    ['data' => t('Type'), 'field' => 'entity_bundle'],
    ['data' => t('Status'), 'field' => 'review_status'],
    ['data' => t('AI Analysis'), 'class' => ['priority-low']],
    ['data' => t('Created'), 'field' => 'created', 'sort' => 'desc'],
    ['data' => t('Operations')],
  ];

  // Build the query.
  $query = db_select('content_lifecycle', 'cl')
    ->extend('PagerDefault')
    ->extend('TableSort');

  $query->fields('cl')
    ->limit(50)
    ->orderByHeader($header);

  // Add filter for status if provided.
  if (!empty($_GET['status'])) {
    $query->condition('review_status', $_GET['status']);
  }

  $result = $query->execute();

  // Build rows.
  $rows = [];
  foreach ($result as $lifecycle) {
    $entity = NULL;
    if ($lifecycle->entity_type && $lifecycle->entity_id) {
      $entities = entity_load($lifecycle->entity_type, [$lifecycle->entity_id]);
      $entity = !empty($entities) ? reset($entities) : NULL;
    }

    // Get content link.
    $content_link = $lifecycle->label;
    if ($entity) {
      $uri = entity_uri($lifecycle->entity_type, $entity);
      if ($uri) {
        $content_link = l($lifecycle->label, $uri['path']);
      }
    }

    // Truncate AI analysis for display.
    $analysis = check_plain($lifecycle->ai_results);
    if (strlen($analysis) > 100) {
      $analysis = substr($analysis, 0, 100) . '...';
    }

    // Get human-readable bundle name.
    $bundle_label = $lifecycle->entity_bundle;
    if ($lifecycle->entity_type == 'node') {
      $node_types = node_type_get_types();
      if (isset($node_types[$lifecycle->entity_bundle])) {
        $bundle_label = $node_types[$lifecycle->entity_bundle]->name;
      }
    }

    // Status with color coding.
    $status_class = 'status-' . str_replace('_', '-', $lifecycle->review_status);
    $status = '<span class="' . $status_class . '">' . check_plain($lifecycle->review_status) . '</span>';

    $rows[] = [
      'data' => [
        $content_link,
        check_plain($bundle_label),
        ['data' => $status, 'class' => [$status_class]],
        $analysis,
        format_date($lifecycle->created, 'short'),
        l(t('View'), 'admin/content/ai-content-lifecycle/' . $lifecycle->id) .
        ' | ' .
        ($entity ? l(t('Edit content'), $uri['path'] . '/edit') : ''),
      ],
    ];
  }

  // Build the output.
  $build = [];

  // Add filter form.
  $build['filters'] = backdrop_get_form('ai_content_lifecycle_filter_form');

  // Add summary.
  $summary = _ai_content_lifecycle_get_summary();
  $build['summary'] = [
    '#markup' => '<div class="lifecycle-summary">' . $summary . '</div>',
  ];

  if (empty($rows)) {
    $build['empty'] = [
      '#markup' => '<p>' . t('No content has been flagged for review yet. Run the <a href="@batch">batch analysis</a> to analyze existing content.', ['@batch' => url('admin/config/ai/ai-content-lifecycle/batch')]) . '</p>',
    ];
  }
  else {
    $build['table'] = [
      '#theme' => 'table',
      '#header' => $header,
      '#rows' => $rows,
      '#attributes' => ['class' => ['content-lifecycle-list']],
    ];

    $build['pager'] = [
      '#theme' => 'pager',
    ];
  }

  // Add CSS.
  backdrop_add_css(backdrop_get_path('module', 'ai_content_lifecycle') . '/css/ai_content_lifecycle.css');

  return $build;
}

/**
 * Filter form for content lifecycle list.
 */
function ai_content_lifecycle_filter_form($form, &$form_state) {
  $form['filters'] = [
    '#type' => 'fieldset',
    '#title' => t('Filter'),
    '#collapsible' => TRUE,
    '#collapsed' => empty($_GET['status']),
  ];

  $form['filters']['status'] = [
    '#type' => 'select',
    '#title' => t('Review Status'),
    '#options' => [
      '' => t('- All -'),
      'pending' => t('Pending'),
      'ok' => t('OK'),
      'analyzed' => t('Analyzed'),
      'reviewed' => t('Reviewed'),
      'updated' => t('Updated'),
      'ignored' => t('Ignored'),
    ],
    '#default_value' => isset($_GET['status']) ? $_GET['status'] : '',
  ];

  $form['filters']['actions'] = [
    '#type' => 'actions',
  ];

  $form['filters']['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Filter'),
  ];

  if (!empty($_GET['status'])) {
    $form['filters']['actions']['reset'] = [
      '#type' => 'submit',
      '#value' => t('Reset'),
      '#submit' => ['ai_content_lifecycle_filter_form_reset'],
    ];
  }

  return $form;
}

/**
 * Submit handler for filter form.
 */
function ai_content_lifecycle_filter_form_submit($form, &$form_state) {
  $status = $form_state['values']['status'];

  if (!empty($status)) {
    $form_state['redirect'] = [
      'admin/content/ai-content-lifecycle',
      ['query' => ['status' => $status]],
    ];
  }
  else {
    $form_state['redirect'] = 'admin/content/ai-content-lifecycle';
  }
}

/**
 * Reset handler for filter form.
 */
function ai_content_lifecycle_filter_form_reset($form, &$form_state) {
  $form_state['redirect'] = 'admin/content/ai-content-lifecycle';
}

/**
 * Get summary statistics.
 */
function _ai_content_lifecycle_get_summary() {
  $statuses = ['pending', 'ok', 'analyzed', 'reviewed', 'updated', 'ignored'];
  $counts = [];
  $total = 0;

  foreach ($statuses as $status) {
    $count = db_query("SELECT COUNT(*) FROM {content_lifecycle} WHERE review_status = :status",
      [':status' => $status])->fetchField();
    $counts[$status] = $count;
    $total += $count;
  }

  $items = [];
  $items[] = '<strong>' . t('Total: @count', ['@count' => $total]) . '</strong>';
  foreach ($counts as $status => $count) {
    if ($count > 0) {
      $label = ucfirst($status) . ': ' . $count;
      $items[] = l($label, 'admin/content/ai-content-lifecycle', ['query' => ['status' => $status]]);
    }
  }

  return implode(' | ', $items);
}

/**
 * Page callback: View a single content lifecycle entity.
 */
function ai_content_lifecycle_view_page($lifecycle_id) {
  $lifecycle = entity_load('content_lifecycle', [$lifecycle_id]);

  if (empty($lifecycle)) {
    return backdrop_not_found();
  }

  $lifecycle = reset($lifecycle);

  // Load the referenced entity.
  $entity = NULL;
  if ($lifecycle->entity_type && $lifecycle->entity_id) {
    $entities = entity_load($lifecycle->entity_type, [$lifecycle->entity_id]);
    $entity = !empty($entities) ? reset($entities) : NULL;
  }

  $build = [];

  // Title.
  backdrop_set_title($lifecycle->label);

  // Status message if entity not found.
  if (!$entity) {
    backdrop_set_message(t('The referenced content no longer exists.'), 'warning');
  }

  // Basic info.
  $build['info'] = [
    '#type' => 'fieldset',
    '#title' => t('Lifecycle Information'),
  ];

  // Get human-readable bundle name.
  $bundle_label = $lifecycle->entity_bundle;
  if ($lifecycle->entity_type == 'node') {
    $node_types = node_type_get_types();
    if (isset($node_types[$lifecycle->entity_bundle])) {
      $bundle_label = $node_types[$lifecycle->entity_bundle]->name;
    }
  }

  $rows = [];
  $rows[] = [t('Status'), '<strong>' . l(check_plain($lifecycle->review_status), 'admin/content/ai-content-lifecycle', ['query' => ['status' => $lifecycle->review_status]]) . '</strong>'];
  $rows[] = [t('Content Type'), check_plain($bundle_label)];

  if ($entity) {
    $uri = entity_uri($lifecycle->entity_type, $entity);
    $rows[] = [t('Content'), l($lifecycle->label, $uri['path'])];
  }
  else {
    $rows[] = [t('Content'), check_plain($lifecycle->label) . ' ' . t('(deleted]')];
  }

  $rows[] = [t('Created'), format_date($lifecycle->created, 'long')];
  $rows[] = [t('Last Updated'), format_date($lifecycle->changed, 'long')];

  $build['info']['table'] = [
    '#theme' => 'table',
    '#rows' => $rows,
    '#attributes' => ['class' => ['lifecycle-info']],
  ];

  // AI Analysis results.
  if (!empty($lifecycle->ai_results)) {
    $build['analysis'] = [
      '#type' => 'fieldset',
      '#title' => t('AI Analysis'),
    ];

    $build['analysis']['results'] = [
      '#markup' => '<div class="ai-results">' . nl2br(check_plain($lifecycle->ai_results)) . '</div>',
    ];
  }

  // Actions.
  $build['actions'] = [
    '#type' => 'fieldset',
    '#title' => t('Actions'),
  ];

  $links = [];

  if ($entity) {
    $uri = entity_uri($lifecycle->entity_type, $entity);
    $links[] = l(t('View content'), $uri['path']);
    $links[] = l(t('Edit content'), $uri['path'] . '/edit');
  }

  $links[] = l(t('Back to list'), 'admin/content/ai-content-lifecycle');

  $build['actions']['links'] = [
    '#markup' => implode(' | ', $links),
  ];

  // Status update form.
  $build['status_form'] = backdrop_get_form('ai_content_lifecycle_status_form', $lifecycle);

  return $build;
}

/**
 * Form to update lifecycle status.
 */
function ai_content_lifecycle_status_form($form, &$form_state, $lifecycle) {
  $form['#lifecycle'] = $lifecycle;

  $form['status'] = [
    '#type' => 'fieldset',
    '#title' => t('Update Status'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  ];

  $form['status']['review_status'] = [
    '#type' => 'select',
    '#title' => t('Review Status'),
    '#options' => [
      'pending' => t('Pending'),
      'ok' => t('OK'),
      'analyzed' => t('Analyzed'),
      'reviewed' => t('Reviewed'),
      'updated' => t('Updated'),
      'ignored' => t('Ignored'),
    ],
    '#default_value' => $lifecycle->review_status,
    '#required' => TRUE,
  ];

  $form['status']['actions'] = [
    '#type' => 'actions',
  ];

  $form['status']['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Update Status'),
  ];

  return $form;
}

/**
 * Submit handler for status update form.
 */
function ai_content_lifecycle_status_form_submit($form, &$form_state) {
  $lifecycle = $form['#lifecycle'];
  $lifecycle->review_status = $form_state['values']['review_status'];
  $lifecycle->save();

  backdrop_set_message(t('Status updated to @status.', ['@status' => $form_state['values']['review_status']]));
  $form_state['redirect'] = 'admin/content/ai-content-lifecycle';
}
