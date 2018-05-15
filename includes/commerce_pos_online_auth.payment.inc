<?php
/**
 * @file
 * Athorized.net Payment Getaway functionality for POS sales.
 */

/**
 * Callback for the POS sale payment form.
 */
function commerce_pos_authorize_net_card_present_page_display($form, &$form_state) {
  commerce_pos_authorize_net_card_present_online($form, $form_state, CommercePosService::TRANSACTION_TYPE_SALE);
  return $form;
}

/**
 * Callback to build a POS payment form.
 */
function commerce_pos_authorize_net_card_present_online(&$form, &$form_state, $transaction_type) {
  global $user;
  form_load_include($form_state, 'inc', 'commerce_pos', 'includes/commerce_pos.common');
  commerce_pos_authorize_net_card_present_ajax_check($form, $form_state);
  $return = $transaction_type == CommercePosService::TRANSACTION_TYPE_RETURN;
  $payment_methods = commerce_pos_get_payment_methods();
  $wrapper_id = 'commerce-pos-pay-form-wrapper';
  $form['#theme'] = 'commerce_pos_payment';
  $form['#redirect_key'] = $return ? 'return' : 'sale';
  $form['#transaction_type'] = $transaction_type;
  $form['#prefix'] = '<div id="' . $wrapper_id . '" class="' . ($return ? 'return' : 'sale') . '">';
  $form['#suffix'] = '</div>';
  $form_state['wrapper_id'] = $wrapper_id;
  $form_ajax = array(
    'wrapper' => 'commerce-pos-pay-form-wrapper',
    'callback' => 'commerce_pos_authorize_net_card_present_payment_wrapper_js',
  );
  if (empty($payment_methods)) {
    drupal_set_message(t('No payment methods have been configured for the POS!'), 'error');
  }
  if (!isset($form_state['transaction'])) {
    if ($transaction = CommercePosService::getCurrentTransaction($transaction_type, $user->uid)) {
      $order_wrapper = $transaction->getOrderWrapper();
      if ($order_wrapper && $order_wrapper->status->value() == 'commerce_pos_in_progress') {
        $form_state['transaction'] = $transaction;
        $form_state['order_wrapper'] = $order_wrapper;
      }
    }
  }
  $form['header'] = array(
    '#markup' => theme('commerce_pos_header', array('account' => $user)),
  );
  if (isset($form_state['transaction'])) {
    $order_value = $form_state['order_wrapper']->value();
    $order_balance = commerce_payment_order_balance($order_value);
    $line_item_count = 0;
    foreach ($form_state['order_wrapper']->commerce_line_items as $line_item_wrapper) {
      if (in_array($line_item_wrapper->type->value(), commerce_product_line_item_types())) {
        $line_item_count++;
      }
    }
    $items_link = l(format_plural($line_item_count, '1 Item', '@count Items'), '', array(
      'fragment' => ' ',
      'external' => TRUE,
      'attributes' => array(
        'class' => array('commerce-pos-summary-toggle'),
      ),
    ));
    $replacements = array(
      '!items' => $items_link,
      '@order_id' => $form_state['order_wrapper']->order_id->value(),
    );
    if ($return) {
      $summary_text = t('<div class="order-text">Return - !items - Order #@order_id </div>', $replacements);
    }
    else {
      $summary_text = t('<div class="order-text">Payment - !items - Order #@order_id </div>', $replacements);
    }
    $form['summary'] = array(
      '#markup' => $summary_text,
    );
    $form['summary_table'] = array(
      '#theme' => 'commerce_pos_product_summary',
      '#order' => $form_state['order_wrapper']->value(),
      '#attributes' => array(
        'class' => array('element-invisible'),
      ),
    );
    $form['edit_order'] = array(
      '#markup' => l($return ? t('Edit Return') : t('Edit Order'), 'pos/' . $form['#redirect_key'], array(
        'attributes' => array(
          'class' => array('commerce-pos-sale-pay-edit-order'),
        ),
      )),
    );
    $form['payment_options'] = array(
      '#type' => 'container',
      '#tree' => TRUE,
    );
    /* $payment_ajax = array(
    'wrapper' => 'commerce-pos-sale-keypad-wrapper',
    'callback' => 'commerce_pos_payment_strip_payment_js',
    'effect' => 'fade',
    );*/
    $form['payment_options']['add_auth_payment'] = array(
      '#type' => 'container',
      '#id' => 'commerce-pos-sale-keypad-wrapper',
      '#prefix' => '<h1>Swipe Card Through Card Reader</h1>',
      '#tree' => TRUE,
    );
    // Attempt to load the billing address from the order data.
    // Differentiate form elements based on stripe integration type.
    // Build a year select list that uses a 4 digit key with a 2 digit value.
    $options = array();
    $current_month = date('m');
    $current_year_2 = date('y');
    $current_year_4 = date('Y');
    for ($i = 0; $i < 20; $i++) {
      $options[$current_year_4 + $i] = str_pad($current_year_2 + $i, 2, '0', STR_PAD_LEFT);
    }
    $form['payment_options']['add_auth_payment']['card_present'] = array(
      '#tree' => TRUE,
      '#attached' => array(
        'css' => array(drupal_get_path('module', 'commerce_payment') . '/theme/commerce_payment.theme.css'),
      ),
    );
    $form['payment_options']['add_auth_payment']['card_present']['track'] = array(
      '#type' => 'textfield',
      '#title' => 'trackdata',
    );
    $form['payment_options']['add_auth_payment']['card_present']['manual_entry'] = array(
      '#type' => 'fieldset',
      '#title' => t('Manual credit card entry'),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    );

    $form['payment_options']['add_auth_payment']['card_present']['manual_entry']['card_num'] = array(
      '#type' => 'textfield',
      '#title' => t('Card number'),
    );

    $form['payment_options']['add_auth_payment']['card_present']['manual_entry']['exp_month'] = array(
      '#type' => 'select',
      '#title' => t('Start date'),
      '#options' => drupal_map_assoc(array_keys(commerce_months())),
      '#default_value' => $current_month,
      '#prefix' => '<div class="commerce-credit-card-start">',
      '#suffix' => '<span class="commerce-month-year-divider">/</span>',
    );
    $form['payment_options']['add_auth_payment']['card_present']['manual_entry']['exp_year'] = array(
      '#type' => 'select',
      '#options' => $options,
      '#default_value' => $current_year_4,
      '#suffix' => '</div>',
    );
    $form['#attached']['js'] = array(
      drupal_get_path('module', 'commerce_authnet_card_present') . '/js/card-swipe.js',
      drupal_get_path('module', 'commerce_pos_authnet_card_present') . '/js/pos_auth_payment.js',
    );
    $form['payment_options']['add_auth_payment']['auth_payment'] = array(
      '#type' => 'submit',
      '#size' => 5,
      '#value' => 'Payment',
      '#submit' => array('commerce_pos_authorize_net_card_present_wrapper_js'),
    );
    $form['balance'] = array(
      '#type' => 'container',
      '#id' => 'commerce-pos-pay-balance-wrapper',
    );

    $order_total = commerce_pos_price_order_format($form_state['order_wrapper'], 'commerce_order_total', $return);
    $form['balance']['order_total'] = array(
      '#markup' => render($order_total),
    );
    $order_value = $form_state['order_wrapper']->value();
    $form['balance']['summary'] = array(
      '#markup' => commerce_pos_balance_summary($order_value),
    );

    $form['balance']['actions'] = array(
      '#type' => 'actions',
    );

    $form['balance']['actions']['remove_payment'] = array(
      '#type' => 'submit',
      '#value' => t('Remove Payment'),
      '#attributes' => array(
        'class' => array(
          'commerce-pos-transaction-btn',
          'commerce-pos-remove-payment',
        ),
        'style' => array('display: none'),
      ),
      '#submit' => array('commerce_pos_authorize_net_card_present_remove_payment_submit'),
      '#ajax' => array_merge($form_ajax, array('event' => 'remove_payment')),
      '#limit_validation_errors' => array(
        array('remove_payment_text'),
      ),
      '#element_key' => 'remove-payment',
    );

    $form['balance']['remove_payment_text'] = array(
      '#type' => 'hidden',
      '#attributes' => array(
        'class' => array(
          'commerce-pos-remove-payment-input',
        ),
        'style' => array('display: none'),
      ),
    );
    $form['balance']['actions']['finish'] = array(
      '#type' => 'submit',
      '#value' => t('Finish'),
      '#disabled' => $order_balance['amount'] > 0,
      '#validate' => array('commerce_pos_authorize_net_card_present_validate'),
      '#submit' => array('commerce_pos_authorize_net_card_present_finish'),
      '#attributes' => array(
        'class' => array('commerce-pos-payment-finish-btn'),
      ),
    );

    $form['balance']['actions']['park'] = array(
      '#type' => 'button',
      '#value' => t('Park'),
      '#attributes' => array(
        'class' => array('commerce-pos-transaction-btn'),
      ),
      '#ajax' => $form_ajax,
      '#limit_validation_errors' => array(),
      '#element_key' => 'park-transaction',
    );

    $form['balance']['actions']['void'] = array(
      '#type' => 'button',
      '#value' => t('Void'),
      '#attributes' => array(
        'class' => array('commerce-pos-transaction-btn'),
      ),
      '#ajax' => $form_ajax,
      '#limit_validation_errors' => array(),
      '#element_key' => 'void-transaction',
    );

    $form['balance']['actions']['customer'] = array(
      '#type' => 'textfield',
      '#title' => t('Find existing customer'),
      '#autocomplete_path' => 'pos/user/autocomplete',
      '#ajax' => array(
        'wrapper' => 'commerce-pos-customer-input-wrapper',
        'callback' => 'commerce_pos_customer_js',
      ),
      '#prefix' => '<div id="commerce-pos-customer-input-wrapper">',
      '#suffix' => '</div>',
      '#element_key' => 'customer-update',
      '#attributes' => array(
        'placeholder' => t('email@address.com'),
      ),
      '#default_value' => !empty($transaction->data['customer email']) ? $transaction->data['customer email'] : NULL,
    );
  }
  else {
    $form['no_transactions'] = array(
      '#markup' => t('You do not currently have any active transactions. Either retrieve a parked transaction or start a new one.'),
    );
  }
  $form['parked_transactions'] = commerce_pos_parked_transactions_section($transaction_type, $form_ajax, !empty($form_state['transaction']));
  $form['#attached']['css'][] = drupal_get_path('module', 'commerce_pos') . '/css/style.css';
  $form['#attached']['js'][] = drupal_get_path('module', 'commerce_pos') . '/js/commerce_pos.payment.js';
  $form['balance']['actions']['customer']['#title'] = '';
  $totalmarkup = $form['balance']['order_total']['#markup'];
  $form['balance']['order_total']['#markup'] = str_replace(array('(excl taxes)'), array(''), $totalmarkup);
  $balncemarkup = $form['balance']['summary']['#markup'];
  $form['balance']['summary']['#markup'] = str_replace('To pay', 'Amount Due', $balncemarkup);
}

/**
 * AJAX callback for the Pay form keypad.
 */
function commerce_pos_authorize_net_card_present_payment_js($form, &$form_state) {
  return $form['payment_options']['add_strip_payment'];
}

/**
 * AJAX callback for the Pay form's wrapper.
 */
function commerce_pos_authorize_net_card_present_payment_wrapper_js($form, &$form_state) {
  return $form;
}

/**
 * AJAX callback for the Pay form keypad.
 */
function commerce_pos_authorize_net_card_present_wrapper_js($form, &$form_state) {
  module_load_include('inc', 'commerce_payment', 'includes/commerce_payment.credit_card');
  // Load and store the payment method instance for this transaction.
  $payment_method = commerce_payment_method_instance_load('authnet_aim_card_present|commerce_payment_authnet_aim_card_present');
  $order_wrapper = $form_state['transaction']->getOrderWrapper();
  $order_id = $order_wrapper->order_id->value();
  $order = commerce_order_load($order_id);
  $charge  = $order->commerce_order_total[LANGUAGE_NONE][0];
  $currency_data = commerce_currency_load($charge['currency_code']);
  // If the charge amount is 0...
  if ($charge['amount'] == 0) {
    // Create a transaction to log the skipped transaction and display a
    // helpful message to the customer.
    $transaction = commerce_payment_transaction_new('authnet_aim_card_present', $order->order_id);
    $transaction->amount = $charge['amount'];
    $transaction->currency_code = $charge['currency_code'];
    $transaction->status = COMMERCE_PAYMENT_STATUS_FAILURE;
    $transaction->message = t('Invalid @amount transaction not attempted.', array('@amount' => commerce_currency_format($charge['amount'], $charge['currency_code'])));
    commerce_payment_transaction_save($transaction);

    drupal_set_message(t('We encountered an error processing your transaction. Please contact us to resolve the issue.'), 'error');
    return FALSE;
  }

  $order_wrapper = entity_metadata_wrapper('commerce_order', $order);
  // Get the default transaction type from the payment method settings.
  $txn_type = $payment_method['settings']['txn_type'];
  // Build a name-value pair array for this transaction.
  $nvp = array(
    'x_type' => commerce_authnet_txn_type($txn_type),
    'x_method' => 'CC',
    'x_mode' => AUTHNET_TXN_MODE_DEVELOPER,
    'x_amount' => number_format(commerce_currency_amount_to_decimal($charge['amount'], $charge['currency_code']), $currency_data['decimals'], $currency_data['decimal_separator'], $currency_data['thousands_separator']),
    'x_currency_code' => $charge['currency_code'],
    'card_present' => TRUE,
  );
  if (!empty($form_state['values']['payment_options']['add_auth_payment']['card_present']['track']) || !empty($form_state['values']['payment_options']['add_auth_payment']['card_present']['manual_entry']['card_num'])) {
    // If there is no track data then fallback to the manual entry option.
    if (!empty($form_state['values']['payment_options']['add_auth_payment']['card_present']['track'])) {
      $pizza = ltrim($form_state['values']['payment_options']['add_auth_payment']['card_present']['track'], '%');
      $track = explode("?;", $pizza);
      $nvp['x_track1'] = $track[0];
    }
    else {
      $exp_date = $form_state['values']['payment_options']['add_auth_payment']['card_present']['manual_entry']['exp_month'] . substr($form_state['values']['payment_options']['add_auth_payment']['card_present']['manual_entry']['exp_year'], 2);
      // Only used when trackdata not present.
      $nvp['x_card_num'] = $form_state['values']['payment_options']['add_auth_payment']['card_present']['manual_entry']['card_num'];
      $nvp['x_exp_date'] = $exp_date;
    }
    // Build a description for the order.
    $description = array();
    // Check for product descriptions. Not all environments have them.
    if (function_exists('commerce_product_line_item_types')) {
      foreach ($order_wrapper->commerce_line_items as $line_item_wrapper) {
        if (in_array($line_item_wrapper->type->value(), commerce_product_line_item_types())) {
          $description[] = round($line_item_wrapper->quantity->value(), 2) . 'x ' . $line_item_wrapper->line_item_label->value();
        }
      }
    }
    // Add additional transaction information to the request array.
    $nvp += array(
      // Order Information.
      'x_invoice_num' => $order->order_number,
      'x_description' => substr(implode(', ', $description), 0, 255),

      // Customer Information.
      'x_email' => substr($order->mail, 0, 255),
      'x_cust_id' => substr($order->uid, 0, 20),
      'x_customer_ip' => substr(ip_address(), 0, 15),
    );

    // Prepare the billing address for use in the request.
    if ($order_wrapper->commerce_customer_billing->value()) {
      $billing_address = $order_wrapper->commerce_customer_billing->commerce_customer_address->value();

      if (empty($billing_address['first_name'])) {
        $name_parts = explode(' ', $billing_address['name_line']);
        $billing_address['first_name'] = array_shift($name_parts);
        $billing_address['last_name'] = implode(' ', $name_parts);
      }

      $nvp += array(
        // Customer Billing Address.
        'x_first_name' => substr($billing_address['first_name'], 0, 50),
        'x_last_name' => substr($billing_address['last_name'], 0, 50),
        'x_company' => substr($billing_address['organisation_name'], 0, 50),
        'x_address' => substr($billing_address['thoroughfare'], 0, 60),
        'x_city' => substr($billing_address['locality'], 0, 40),
        'x_state' => substr($billing_address['administrative_area'], 0, 40),
        'x_zip' => substr($billing_address['postal_code'], 0, 20),
        'x_country' => $billing_address['country'],
      );
    }
    else {
      $billing_address = array();
    }

    // Submit the request to Authorize.Net.
    $response = commerce_authnet_aim_request($payment_method, $nvp);
    // Prepare a transaction object to log the API response.
    $transaction = commerce_payment_transaction_new('authnet_aim_card_present', $order->order_id);
    $transaction->instance_id = 'authnet_aim_card_present|commerce_payment_authnet_aim_card_present';
    $transaction->remote_id = $response[7];
    $transaction->amount = $charge['amount'];
    $transaction->currency_code = $charge['currency_code'];
    $transaction->payload[REQUEST_TIME] = $response;

    // If we didn't get an approval response code...
    if ($response[COMMERCE_AUTHNET_CARD_PRESENT_RESPONSE_CODE_LOC] != '1') {
      // Create a failed transaction with the error message.
      $transaction->status = COMMERCE_PAYMENT_STATUS_FAILURE;
    }
    else {
      // Set the transaction status based on the type of transaction this was.
      switch ($txn_type) {
        case COMMERCE_CREDIT_AUTH_ONLY:
          $transaction->status = COMMERCE_PAYMENT_STATUS_PENDING;
          break;

        case COMMERCE_CREDIT_AUTH_CAPTURE:
          $transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;
          break;

        case COMMERCE_CREDIT_CAPTURE_ONLY:
          $transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;
          break;
      }
    }

    // Store the type of transaction in the remote status.
    $transaction->remote_status = $response[11];

    // Build a meaningful response message.
    $message = array(
      '<b>' . commerce_authnet_reverse_txn_type($response[11]) . '</b>',
      '<b>' . ($response[COMMERCE_AUTHNET_CARD_PRESENT_RESPONSE_CODE_LOC] != '1' ? t('REJECTED') : t('ACCEPTED')) . ':</b> ' . check_plain($response[3]),
      t('AVS response: @avs', array('@avs' => commerce_authnet_avs_response($response[5]))),
    );

    // Add the CVV response if enabled.
    if (isset($nvp['x_card_code'])) {
      $message[] = t('CVV match: @cvv', array('@cvv' => commerce_authnet_cvv_response($response[38])));
    }
    $transaction->message = implode('<br />', $message);
    // Save the transaction information.
    commerce_payment_transaction_save($transaction);
    // If the payment failed, display an error and rebuild the form.
    if ($response[COMMERCE_AUTHNET_CARD_PRESENT_RESPONSE_CODE_LOC] != '1') {
      drupal_set_message(t('We received the following error processing your card. Please enter your information again or try a different card.'), 'error');
      drupal_set_message(check_plain($response[3]), 'error');
      return FALSE;
    }
  }
  else {
    drupal_set_message(t('Please enter your information again or try a different card.'), 'error');
  }
  $form_state['values']['keypad']['amount'] = NULL;
  $form_state['triggering_element']['#payment_method_id'] = NULL;
  $form_state['values']['payment_options']['add_auth_payment']['card_present']['track'] = NULL;
  $form_state['rebuild'] = TRUE;
}

/**
 * Validation handler for the pay form's "Finish" button.
 */
function commerce_pos_authorize_net_card_present_validate($form, &$form_state) {

}

/**
 * Validation handler for the pay form's "Finish" button.
 */
function commerce_pos_authorize_net_card_present_remove_payment_submit($form, &$form_state) {
  $transaction_id = trim($form_state['values']['remove_payment_text']);
  commerce_payment_transaction_delete($transaction_id);
  $form_state['rebuild'] = TRUE;
}

/**
 * Submit handler for the pay form's "Finish" button.
 */
function commerce_pos_authorize_net_card_present_finish($form, &$form_state) {
  $form_state['transaction']->doAction('completeTransaction');
  $form_state['redirect'] = 'pos/' . $form['#redirect_key'];
  drupal_set_message(t('Transaction Completed'));
}

/**
 * Helper function to check for AJAX submissions on the POS pay form.
 *
 * This will look at what triggered the AJAX submission and act accordingly.
 */
function commerce_pos_authorize_net_card_present_ajax_check($form, &$form_state) {
  if (isset($form_state['triggering_element'])) {
    $triggering_element = $form_state['triggering_element'];
    if (!empty($form_state['triggering_element']['#element_key'])) {
      /* @var CommercePosTransaction $transaction */
      $transaction = $form_state['transaction'];
      $form_state['order_updated'] = TRUE;
      switch ($triggering_element['#element_key']) {
        case 'retrieve-parked-transaction':
          $parked_transaction = CommercePosService::loadTransaction($triggering_element['#transaction_id']);
          $parked_transaction->unpark();
          if ($transaction) {
            $transaction->void();
          }
          unset($form_state['transaction']);
          drupal_set_message(t('Transaction @id retrieved.', array('@id' => $parked_transaction->transactionId)));
          break;

        case 'park-transaction':
          $transaction->doAction('park');
          unset($form_state['transaction']);
          drupal_set_message(t('Transaction @id parked.', array('@id' => $transaction->transactionId)));
          break;

        case 'void-transaction':
          $transaction->void();
          unset($form_state['transaction']);
          drupal_set_message(t('Transaction @id voided.', array('@id' => $transaction->transactionId)));
          break;

        case 'customer-update':
          $transaction->data['customer email'] = !empty($triggering_element['#value']) ? $triggering_element['#value'] : NULL;
          $transaction->doAction('save');
          break;

        default:
          $form_state['order_updated'] = FALSE;
      }
    }
  }
}
