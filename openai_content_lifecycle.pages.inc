<?php

/**
 * @file
 * Page callbacks for OpenAI Content Lifecycle module.
 */

/**
 * Page callback: List all content lifecycle entities.
 */
function openai_content_lifecycle_list_page() {
  $header = array(
    array('data' => t('Content'), 'field' => 'label'),
    array('data' => t('Type'), 'field' => 'entity_bundle'),
    array('data' => t('Status'), 'field' => 'review_status'),
    array('data' => t('AI Analysis'), 'class' => array('priority-low')),
    array('data' => t('Created'), 'field' => 'created', 'sort' => 'desc'),
    array('data' => t('Operations')),
  );

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
  $rows = array();
  foreach ($result as $lifecycle) {
    $entity = NULL;
    if ($lifecycle->entity_type && $lifecycle->entity_id) {
      $entities = entity_load($lifecycle->entity_type, array($lifecycle->entity_id));
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

    $rows[] = array(
      'data' => array(
        $content_link,
        check_plain($bundle_label),
        array('data' => $status, 'class' => array($status_class)),
        $analysis,
        format_date($lifecycle->created, 'short'),
        l(t('View'), 'admin/content/content-lifecycle/' . $lifecycle->id) . ' | ' .
        ($entity ? l(t('Edit content'), $uri['path'] . '/edit') : ''),
      ),
    );
  }

  // Build the output.
  $build = array();

  // Add filter form.
  $build['filters'] = backdrop_get_form('openai_content_lifecycle_filter_form');

  // Add summary.
  $summary = _openai_content_lifecycle_get_summary();
  $build['summary'] = array(
    '#markup' => '<div class="lifecycle-summary">' . $summary . '</div>',
  );

  if (empty($rows)) {
    $build['empty'] = array(
      '#markup' => '<p>' . t('No content has been flagged for review yet. Run the <a href="@batch">batch analysis</a> to analyze existing content.', array('@batch' => url('admin/config/content/openai-content-lifecycle/batch'))) . '</p>',
    );
  }
  else {
    $build['table'] = array(
      '#theme' => 'table',
      '#header' => $header,
      '#rows' => $rows,
      '#attributes' => array('class' => array('content-lifecycle-list')),
    );

    $build['pager'] = array(
      '#theme' => 'pager',
    );
  }

  // Add CSS.
  backdrop_add_css(backdrop_get_path('module', 'openai_content_lifecycle') . '/css/openai_content_lifecycle.css');

  return $build;
}

/**
 * Filter form for content lifecycle list.
 */
function openai_content_lifecycle_filter_form($form, &$form_state) {
  $form['filters'] = array(
    '#type' => 'fieldset',
    '#title' => t('Filter'),
    '#collapsible' => TRUE,
    '#collapsed' => empty($_GET['status']),
  );

  $form['filters']['status'] = array(
    '#type' => 'select',
    '#title' => t('Review Status'),
    '#options' => array(
      '' => t('- All -'),
      'pending' => t('Pending'),
      'analyzed' => t('Analyzed'),
      'reviewed' => t('Reviewed'),
      'updated' => t('Updated'),
      'ignored' => t('Ignored'),
    ),
    '#default_value' => isset($_GET['status']) ? $_GET['status'] : '',
  );

  $form['filters']['actions'] = array(
    '#type' => 'actions',
  );

  $form['filters']['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Filter'),
  );

  if (!empty($_GET['status'])) {
    $form['filters']['actions']['reset'] = array(
      '#type' => 'submit',
      '#value' => t('Reset'),
      '#submit' => array('openai_content_lifecycle_filter_form_reset'),
    );
  }

  return $form;
}

/**
 * Submit handler for filter form.
 */
function openai_content_lifecycle_filter_form_submit($form, &$form_state) {
  $status = $form_state['values']['status'];

  if (!empty($status)) {
    $form_state['redirect'] = array(
      'admin/content/content-lifecycle',
      array('query' => array('status' => $status)),
    );
  }
  else {
    $form_state['redirect'] = 'admin/content/content-lifecycle';
  }
}

/**
 * Reset handler for filter form.
 */
function openai_content_lifecycle_filter_form_reset($form, &$form_state) {
  $form_state['redirect'] = 'admin/content/content-lifecycle';
}

/**
 * Get summary statistics.
 */
function _openai_content_lifecycle_get_summary() {
  $statuses = array('pending', 'analyzed', 'reviewed', 'updated', 'ignored');
  $counts = array();
  $total = 0;

  foreach ($statuses as $status) {
    $count = db_query("SELECT COUNT(*) FROM {content_lifecycle} WHERE review_status = :status",
      array(':status' => $status))->fetchField();
    $counts[$status] = $count;
    $total += $count;
  }

  $items = array();
  $items[] = '<strong>' . t('Total: @count', array('@count' => $total)) . '</strong>';
  foreach ($counts as $status => $count) {
    if ($count > 0) {
      $items[] = t('@status: @count', array('@status' => ucfirst($status), '@count' => $count));
    }
  }

  return implode(' | ', $items);
}

/**
 * Page callback: View a single content lifecycle entity.
 */
function openai_content_lifecycle_view_page($lifecycle_id) {
  $lifecycle = entity_load('content_lifecycle', array($lifecycle_id));

  if (empty($lifecycle)) {
    backdrop_not_found();
    return;
  }

  $lifecycle = reset($lifecycle);

  // Load the referenced entity.
  $entity = NULL;
  if ($lifecycle->entity_type && $lifecycle->entity_id) {
    $entities = entity_load($lifecycle->entity_type, array($lifecycle->entity_id));
    $entity = !empty($entities) ? reset($entities) : NULL;
  }

  $build = array();

  // Title.
  backdrop_set_title($lifecycle->label);

  // Status message if entity not found.
  if (!$entity) {
    backdrop_set_message(t('The referenced content no longer exists.'), 'warning');
  }

  // Basic info.
  $build['info'] = array(
    '#type' => 'fieldset',
    '#title' => t('Lifecycle Information'),
  );

  // Get human-readable bundle name.
  $bundle_label = $lifecycle->entity_bundle;
  if ($lifecycle->entity_type == 'node') {
    $node_types = node_type_get_types();
    if (isset($node_types[$lifecycle->entity_bundle])) {
      $bundle_label = $node_types[$lifecycle->entity_bundle]->name;
    }
  }

  $rows = array();
  $rows[] = array(t('Status'), '<strong>' . check_plain($lifecycle->review_status) . '</strong>');
  $rows[] = array(t('Content Type'), check_plain($bundle_label));

  if ($entity) {
    $uri = entity_uri($lifecycle->entity_type, $entity);
    $rows[] = array(t('Content'), l($lifecycle->label, $uri['path']));
  }
  else {
    $rows[] = array(t('Content'), check_plain($lifecycle->label) . ' ' . t('(deleted)'));
  }

  $rows[] = array(t('Created'), format_date($lifecycle->created, 'long'));
  $rows[] = array(t('Last Updated'), format_date($lifecycle->changed, 'long'));

  $build['info']['table'] = array(
    '#theme' => 'table',
    '#rows' => $rows,
    '#attributes' => array('class' => array('lifecycle-info')),
  );

  // AI Analysis results.
  if (!empty($lifecycle->ai_results)) {
    $build['analysis'] = array(
      '#type' => 'fieldset',
      '#title' => t('AI Analysis'),
    );

    $build['analysis']['results'] = array(
      '#markup' => '<div class="ai-results">' . nl2br(check_plain($lifecycle->ai_results)) . '</div>',
    );
  }

  // Actions.
  $build['actions'] = array(
    '#type' => 'fieldset',
    '#title' => t('Actions'),
  );

  $links = array();

  if ($entity) {
    $uri = entity_uri($lifecycle->entity_type, $entity);
    $links[] = l(t('View content'), $uri['path']);
    $links[] = l(t('Edit content'), $uri['path'] . '/edit');
  }

  $links[] = l(t('Back to list'), 'admin/content/content-lifecycle');

  $build['actions']['links'] = array(
    '#markup' => implode(' | ', $links),
  );

  // Status update form.
  $build['status_form'] = backdrop_get_form('openai_content_lifecycle_status_form', $lifecycle);

  return $build;
}

/**
 * Form to update lifecycle status.
 */
function openai_content_lifecycle_status_form($form, &$form_state, $lifecycle) {
  $form['#lifecycle'] = $lifecycle;

  $form['status'] = array(
    '#type' => 'fieldset',
    '#title' => t('Update Status'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );

  $form['status']['review_status'] = array(
    '#type' => 'select',
    '#title' => t('Review Status'),
    '#options' => array(
      'pending' => t('Pending'),
      'analyzed' => t('Analyzed'),
      'reviewed' => t('Reviewed'),
      'updated' => t('Updated'),
      'ignored' => t('Ignored'),
    ),
    '#default_value' => $lifecycle->review_status,
    '#required' => TRUE,
  );

  $form['status']['actions'] = array(
    '#type' => 'actions',
  );

  $form['status']['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Update Status'),
  );

  return $form;
}

/**
 * Submit handler for status update form.
 */
function openai_content_lifecycle_status_form_submit($form, &$form_state) {
  $lifecycle = $form['#lifecycle'];
  $lifecycle->review_status = $form_state['values']['review_status'];
  $lifecycle->save();

  backdrop_set_message(t('Status updated to @status.', array('@status' => $form_state['values']['review_status'])));
  $form_state['redirect'] = 'admin/content/content-lifecycle';
}
