.. _gpio_handling_sample:

GPIO interrupt handling
#######################

.. contents::
   :local:
   :depth: 2

The GPIO interrupt handling sample demonstrates how pin state change can be handled by the software.

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

.. include:: /includes/tfm.txt

The sample also requires the following pins to be shorted:

* 0.4 pin and 0.26 pin on nrf5340dk

Overview
********

The sample implements a simple loopback using a single UART instance.
By default, the console and logging are disabled to demonstrate low power consumption when UART is active.

Configuration
*************

|config|

FEM support
===========

.. include:: /includes/sample_fem_support.txt

Building and running
********************
.. |sample path| replace:: :file:`samples/peripheral/gpio_handling`

.. include:: /includes/build_and_run_ns.txt

Testing
=======

After programming the sample to your development kit, test it by performing the following steps:

1. Flash the board.
2. Read logging output which presents latency for various methods of GPIO pin state change handling.

Dependencies
************

It uses the following Zephyr libraries:

* :ref:`zephyr:device_model_api`
* :ref:`zephyr:logging_api`
* :ref:`zephyr:gpio`
