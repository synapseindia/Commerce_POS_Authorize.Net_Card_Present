/**
 * @file
 * Javascript to generate Stripe token in PCI-compliant way.
 */

(function ($) {
  'use strict';
  Drupal.behaviors.PosStripPayment = {
    attach: function (context, settings) {
      $('#edit-payment-options-add-auth-payment-card-present-track', context).focus();
    }
  };
})(jQuery);
