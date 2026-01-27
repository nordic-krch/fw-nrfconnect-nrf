#ifndef PPI_SEQ_SPI_SRC_RTC_H__
#define PPI_SEQ_SPI_SRC_RTC_H__

#if !defined(__ASSEMBLER__) && defined(NRF54L15_XXAA)

#ifdef __cplusplus
extern "C" {
#endif

#define RTC_PRESENT 1

/* Peripheral: RTC */
/* Description: Real-time counter 0 */

/* Register: RTC_TASKS_START */
/* Description: Start RTC counter */

/* Bit 0 : Start RTC counter */
#define RTC_TASKS_START_TASKS_START_Pos (0UL) /*!< Position of TASKS_START field. */
#define RTC_TASKS_START_TASKS_START_Msk (0x1UL << RTC_TASKS_START_TASKS_START_Pos) /*!< Bit mask of TASKS_START field. */
#define RTC_TASKS_START_TASKS_START_Trigger (0x1UL) /*!< Trigger task */

/* Register: RTC_TASKS_STOP */
/* Description: Stop RTC counter */

/* Bit 0 : Stop RTC counter */
#define RTC_TASKS_STOP_TASKS_STOP_Pos (0UL) /*!< Position of TASKS_STOP field. */
#define RTC_TASKS_STOP_TASKS_STOP_Msk (0x1UL << RTC_TASKS_STOP_TASKS_STOP_Pos) /*!< Bit mask of TASKS_STOP field. */
#define RTC_TASKS_STOP_TASKS_STOP_Trigger (0x1UL) /*!< Trigger task */

/* Register: RTC_TASKS_CLEAR */
/* Description: Clear RTC counter */

/* Bit 0 : Clear RTC counter */
#define RTC_TASKS_CLEAR_TASKS_CLEAR_Pos (0UL) /*!< Position of TASKS_CLEAR field. */
#define RTC_TASKS_CLEAR_TASKS_CLEAR_Msk (0x1UL << RTC_TASKS_CLEAR_TASKS_CLEAR_Pos) /*!< Bit mask of TASKS_CLEAR field. */
#define RTC_TASKS_CLEAR_TASKS_CLEAR_Trigger (0x1UL) /*!< Trigger task */

/* Register: RTC_TASKS_TRIGOVRFLW */
/* Description: Set counter to 0xFFFFF0 */

/* Bit 0 : Set counter to 0xFFFFF0 */
#define RTC_TASKS_TRIGOVRFLW_TASKS_TRIGOVRFLW_Pos (0UL) /*!< Position of TASKS_TRIGOVRFLW field. */
#define RTC_TASKS_TRIGOVRFLW_TASKS_TRIGOVRFLW_Msk (0x1UL << RTC_TASKS_TRIGOVRFLW_TASKS_TRIGOVRFLW_Pos) /*!< Bit mask of TASKS_TRIGOVRFLW field. */
#define RTC_TASKS_TRIGOVRFLW_TASKS_TRIGOVRFLW_Trigger (0x1UL) /*!< Trigger task */

/* Register: RTC_TASKS_CAPTURE */
/* Description: Description collection: Capture RTC counter to CC[n] register */

/* Bit 0 : Capture RTC counter to CC[n] register */
#define RTC_TASKS_CAPTURE_TASKS_CAPTURE_Pos (0UL) /*!< Position of TASKS_CAPTURE field. */
#define RTC_TASKS_CAPTURE_TASKS_CAPTURE_Msk (0x1UL << RTC_TASKS_CAPTURE_TASKS_CAPTURE_Pos) /*!< Bit mask of TASKS_CAPTURE field. */
#define RTC_TASKS_CAPTURE_TASKS_CAPTURE_Trigger (0x1UL) /*!< Trigger task */

/* Register: RTC_SUBSCRIBE_START */
/* Description: Subscribe configuration for task START */

/* Bit 31 :   */
#define RTC_SUBSCRIBE_START_EN_Pos (31UL) /*!< Position of EN field. */
#define RTC_SUBSCRIBE_START_EN_Msk (0x1UL << RTC_SUBSCRIBE_START_EN_Pos) /*!< Bit mask of EN field. */
#define RTC_SUBSCRIBE_START_EN_Disabled (0x0UL) /*!< Disable subscription */
#define RTC_SUBSCRIBE_START_EN_Enabled (0x1UL) /*!< Enable subscription */

/* Bits 7..0 : DPPI channel that task START will subscribe to */
#define RTC_SUBSCRIBE_START_CHIDX_Pos (0UL) /*!< Position of CHIDX field. */
#define RTC_SUBSCRIBE_START_CHIDX_Msk (0xFFUL << RTC_SUBSCRIBE_START_CHIDX_Pos) /*!< Bit mask of CHIDX field. */

/* Register: RTC_SUBSCRIBE_STOP */
/* Description: Subscribe configuration for task STOP */

/* Bit 31 :   */
#define RTC_SUBSCRIBE_STOP_EN_Pos (31UL) /*!< Position of EN field. */
#define RTC_SUBSCRIBE_STOP_EN_Msk (0x1UL << RTC_SUBSCRIBE_STOP_EN_Pos) /*!< Bit mask of EN field. */
#define RTC_SUBSCRIBE_STOP_EN_Disabled (0x0UL) /*!< Disable subscription */
#define RTC_SUBSCRIBE_STOP_EN_Enabled (0x1UL) /*!< Enable subscription */

/* Bits 7..0 : DPPI channel that task STOP will subscribe to */
#define RTC_SUBSCRIBE_STOP_CHIDX_Pos (0UL) /*!< Position of CHIDX field. */
#define RTC_SUBSCRIBE_STOP_CHIDX_Msk (0xFFUL << RTC_SUBSCRIBE_STOP_CHIDX_Pos) /*!< Bit mask of CHIDX field. */

/* Register: RTC_SUBSCRIBE_CLEAR */
/* Description: Subscribe configuration for task CLEAR */

/* Bit 31 :   */
#define RTC_SUBSCRIBE_CLEAR_EN_Pos (31UL) /*!< Position of EN field. */
#define RTC_SUBSCRIBE_CLEAR_EN_Msk (0x1UL << RTC_SUBSCRIBE_CLEAR_EN_Pos) /*!< Bit mask of EN field. */
#define RTC_SUBSCRIBE_CLEAR_EN_Disabled (0x0UL) /*!< Disable subscription */
#define RTC_SUBSCRIBE_CLEAR_EN_Enabled (0x1UL) /*!< Enable subscription */

/* Bits 7..0 : DPPI channel that task CLEAR will subscribe to */
#define RTC_SUBSCRIBE_CLEAR_CHIDX_Pos (0UL) /*!< Position of CHIDX field. */
#define RTC_SUBSCRIBE_CLEAR_CHIDX_Msk (0xFFUL << RTC_SUBSCRIBE_CLEAR_CHIDX_Pos) /*!< Bit mask of CHIDX field. */

/* Register: RTC_SUBSCRIBE_TRIGOVRFLW */
/* Description: Subscribe configuration for task TRIGOVRFLW */

/* Bit 31 :   */
#define RTC_SUBSCRIBE_TRIGOVRFLW_EN_Pos (31UL) /*!< Position of EN field. */
#define RTC_SUBSCRIBE_TRIGOVRFLW_EN_Msk (0x1UL << RTC_SUBSCRIBE_TRIGOVRFLW_EN_Pos) /*!< Bit mask of EN field. */
#define RTC_SUBSCRIBE_TRIGOVRFLW_EN_Disabled (0x0UL) /*!< Disable subscription */
#define RTC_SUBSCRIBE_TRIGOVRFLW_EN_Enabled (0x1UL) /*!< Enable subscription */

/* Bits 7..0 : DPPI channel that task TRIGOVRFLW will subscribe to */
#define RTC_SUBSCRIBE_TRIGOVRFLW_CHIDX_Pos (0UL) /*!< Position of CHIDX field. */
#define RTC_SUBSCRIBE_TRIGOVRFLW_CHIDX_Msk (0xFFUL << RTC_SUBSCRIBE_TRIGOVRFLW_CHIDX_Pos) /*!< Bit mask of CHIDX field. */

/* Register: RTC_SUBSCRIBE_CAPTURE */
/* Description: Description collection: Subscribe configuration for task CAPTURE[n] */

/* Bit 31 :   */
#define RTC_SUBSCRIBE_CAPTURE_EN_Pos (31UL) /*!< Position of EN field. */
#define RTC_SUBSCRIBE_CAPTURE_EN_Msk (0x1UL << RTC_SUBSCRIBE_CAPTURE_EN_Pos) /*!< Bit mask of EN field. */
#define RTC_SUBSCRIBE_CAPTURE_EN_Disabled (0x0UL) /*!< Disable subscription */
#define RTC_SUBSCRIBE_CAPTURE_EN_Enabled (0x1UL) /*!< Enable subscription */

/* Bits 7..0 : DPPI channel that task CAPTURE[n] will subscribe to */
#define RTC_SUBSCRIBE_CAPTURE_CHIDX_Pos (0UL) /*!< Position of CHIDX field. */
#define RTC_SUBSCRIBE_CAPTURE_CHIDX_Msk (0xFFUL << RTC_SUBSCRIBE_CAPTURE_CHIDX_Pos) /*!< Bit mask of CHIDX field. */

/* Register: RTC_EVENTS_TICK */
/* Description: Event on counter increment */

/* Bit 0 : Event on counter increment */
#define RTC_EVENTS_TICK_EVENTS_TICK_Pos (0UL) /*!< Position of EVENTS_TICK field. */
#define RTC_EVENTS_TICK_EVENTS_TICK_Msk (0x1UL << RTC_EVENTS_TICK_EVENTS_TICK_Pos) /*!< Bit mask of EVENTS_TICK field. */
#define RTC_EVENTS_TICK_EVENTS_TICK_NotGenerated (0x0UL) /*!< Event not generated */
#define RTC_EVENTS_TICK_EVENTS_TICK_Generated (0x1UL) /*!< Event generated */

/* Register: RTC_EVENTS_OVRFLW */
/* Description: Event on counter overflow */

/* Bit 0 : Event on counter overflow */
#define RTC_EVENTS_OVRFLW_EVENTS_OVRFLW_Pos (0UL) /*!< Position of EVENTS_OVRFLW field. */
#define RTC_EVENTS_OVRFLW_EVENTS_OVRFLW_Msk (0x1UL << RTC_EVENTS_OVRFLW_EVENTS_OVRFLW_Pos) /*!< Bit mask of EVENTS_OVRFLW field. */
#define RTC_EVENTS_OVRFLW_EVENTS_OVRFLW_NotGenerated (0x0UL) /*!< Event not generated */
#define RTC_EVENTS_OVRFLW_EVENTS_OVRFLW_Generated (0x1UL) /*!< Event generated */

/* Register: RTC_EVENTS_COMPARE */
/* Description: Description collection: Compare event on CC[n] match */

/* Bit 0 : Compare event on CC[n] match */
#define RTC_EVENTS_COMPARE_EVENTS_COMPARE_Pos (0UL) /*!< Position of EVENTS_COMPARE field. */
#define RTC_EVENTS_COMPARE_EVENTS_COMPARE_Msk (0x1UL << RTC_EVENTS_COMPARE_EVENTS_COMPARE_Pos) /*!< Bit mask of EVENTS_COMPARE field. */
#define RTC_EVENTS_COMPARE_EVENTS_COMPARE_NotGenerated (0x0UL) /*!< Event not generated */
#define RTC_EVENTS_COMPARE_EVENTS_COMPARE_Generated (0x1UL) /*!< Event generated */

/* Register: RTC_PUBLISH_TICK */
/* Description: Publish configuration for event TICK */

/* Bit 31 :   */
#define RTC_PUBLISH_TICK_EN_Pos (31UL) /*!< Position of EN field. */
#define RTC_PUBLISH_TICK_EN_Msk (0x1UL << RTC_PUBLISH_TICK_EN_Pos) /*!< Bit mask of EN field. */
#define RTC_PUBLISH_TICK_EN_Disabled (0x0UL) /*!< Disable publishing */
#define RTC_PUBLISH_TICK_EN_Enabled (0x1UL) /*!< Enable publishing */

/* Bits 7..0 : DPPI channel that event TICK will publish to */
#define RTC_PUBLISH_TICK_CHIDX_Pos (0UL) /*!< Position of CHIDX field. */
#define RTC_PUBLISH_TICK_CHIDX_Msk (0xFFUL << RTC_PUBLISH_TICK_CHIDX_Pos) /*!< Bit mask of CHIDX field. */

/* Register: RTC_PUBLISH_OVRFLW */
/* Description: Publish configuration for event OVRFLW */

/* Bit 31 :   */
#define RTC_PUBLISH_OVRFLW_EN_Pos (31UL) /*!< Position of EN field. */
#define RTC_PUBLISH_OVRFLW_EN_Msk (0x1UL << RTC_PUBLISH_OVRFLW_EN_Pos) /*!< Bit mask of EN field. */
#define RTC_PUBLISH_OVRFLW_EN_Disabled (0x0UL) /*!< Disable publishing */
#define RTC_PUBLISH_OVRFLW_EN_Enabled (0x1UL) /*!< Enable publishing */

/* Bits 7..0 : DPPI channel that event OVRFLW will publish to */
#define RTC_PUBLISH_OVRFLW_CHIDX_Pos (0UL) /*!< Position of CHIDX field. */
#define RTC_PUBLISH_OVRFLW_CHIDX_Msk (0xFFUL << RTC_PUBLISH_OVRFLW_CHIDX_Pos) /*!< Bit mask of CHIDX field. */

/* Register: RTC_PUBLISH_COMPARE */
/* Description: Description collection: Publish configuration for event COMPARE[n] */

/* Bit 31 :   */
#define RTC_PUBLISH_COMPARE_EN_Pos (31UL) /*!< Position of EN field. */
#define RTC_PUBLISH_COMPARE_EN_Msk (0x1UL << RTC_PUBLISH_COMPARE_EN_Pos) /*!< Bit mask of EN field. */
#define RTC_PUBLISH_COMPARE_EN_Disabled (0x0UL) /*!< Disable publishing */
#define RTC_PUBLISH_COMPARE_EN_Enabled (0x1UL) /*!< Enable publishing */

/* Bits 7..0 : DPPI channel that event COMPARE[n] will publish to */
#define RTC_PUBLISH_COMPARE_CHIDX_Pos (0UL) /*!< Position of CHIDX field. */
#define RTC_PUBLISH_COMPARE_CHIDX_Msk (0xFFUL << RTC_PUBLISH_COMPARE_CHIDX_Pos) /*!< Bit mask of CHIDX field. */

/* Register: RTC_SHORTS */
/* Description: Shortcuts between local events and tasks */

/* Bit 3 : Shortcut between event COMPARE[3] and task CLEAR */
#define RTC_SHORTS_COMPARE3_CLEAR_Pos (3UL) /*!< Position of COMPARE3_CLEAR field. */
#define RTC_SHORTS_COMPARE3_CLEAR_Msk (0x1UL << RTC_SHORTS_COMPARE3_CLEAR_Pos) /*!< Bit mask of COMPARE3_CLEAR field. */
#define RTC_SHORTS_COMPARE3_CLEAR_Disabled (0x0UL) /*!< Disable shortcut */
#define RTC_SHORTS_COMPARE3_CLEAR_Enabled (0x1UL) /*!< Enable shortcut */

/* Bit 2 : Shortcut between event COMPARE[2] and task CLEAR */
#define RTC_SHORTS_COMPARE2_CLEAR_Pos (2UL) /*!< Position of COMPARE2_CLEAR field. */
#define RTC_SHORTS_COMPARE2_CLEAR_Msk (0x1UL << RTC_SHORTS_COMPARE2_CLEAR_Pos) /*!< Bit mask of COMPARE2_CLEAR field. */
#define RTC_SHORTS_COMPARE2_CLEAR_Disabled (0x0UL) /*!< Disable shortcut */
#define RTC_SHORTS_COMPARE2_CLEAR_Enabled (0x1UL) /*!< Enable shortcut */

/* Bit 1 : Shortcut between event COMPARE[1] and task CLEAR */
#define RTC_SHORTS_COMPARE1_CLEAR_Pos (1UL) /*!< Position of COMPARE1_CLEAR field. */
#define RTC_SHORTS_COMPARE1_CLEAR_Msk (0x1UL << RTC_SHORTS_COMPARE1_CLEAR_Pos) /*!< Bit mask of COMPARE1_CLEAR field. */
#define RTC_SHORTS_COMPARE1_CLEAR_Disabled (0x0UL) /*!< Disable shortcut */
#define RTC_SHORTS_COMPARE1_CLEAR_Enabled (0x1UL) /*!< Enable shortcut */

/* Bit 0 : Shortcut between event COMPARE[0] and task CLEAR */
#define RTC_SHORTS_COMPARE0_CLEAR_Pos (0UL) /*!< Position of COMPARE0_CLEAR field. */
#define RTC_SHORTS_COMPARE0_CLEAR_Msk (0x1UL << RTC_SHORTS_COMPARE0_CLEAR_Pos) /*!< Bit mask of COMPARE0_CLEAR field. */
#define RTC_SHORTS_COMPARE0_CLEAR_Disabled (0x0UL) /*!< Disable shortcut */
#define RTC_SHORTS_COMPARE0_CLEAR_Enabled (0x1UL) /*!< Enable shortcut */

/* Register: RTC_INTENSET */
/* Description: Enable interrupt */

/* Bit 19 : Write '1' to enable interrupt for event COMPARE[3] */
#define RTC_INTENSET_COMPARE3_Pos (19UL) /*!< Position of COMPARE3 field. */
#define RTC_INTENSET_COMPARE3_Msk (0x1UL << RTC_INTENSET_COMPARE3_Pos) /*!< Bit mask of COMPARE3 field. */
#define RTC_INTENSET_COMPARE3_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_INTENSET_COMPARE3_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_INTENSET_COMPARE3_Set (0x1UL) /*!< Enable */

/* Bit 18 : Write '1' to enable interrupt for event COMPARE[2] */
#define RTC_INTENSET_COMPARE2_Pos (18UL) /*!< Position of COMPARE2 field. */
#define RTC_INTENSET_COMPARE2_Msk (0x1UL << RTC_INTENSET_COMPARE2_Pos) /*!< Bit mask of COMPARE2 field. */
#define RTC_INTENSET_COMPARE2_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_INTENSET_COMPARE2_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_INTENSET_COMPARE2_Set (0x1UL) /*!< Enable */

/* Bit 17 : Write '1' to enable interrupt for event COMPARE[1] */
#define RTC_INTENSET_COMPARE1_Pos (17UL) /*!< Position of COMPARE1 field. */
#define RTC_INTENSET_COMPARE1_Msk (0x1UL << RTC_INTENSET_COMPARE1_Pos) /*!< Bit mask of COMPARE1 field. */
#define RTC_INTENSET_COMPARE1_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_INTENSET_COMPARE1_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_INTENSET_COMPARE1_Set (0x1UL) /*!< Enable */

/* Bit 16 : Write '1' to enable interrupt for event COMPARE[0] */
#define RTC_INTENSET_COMPARE0_Pos (16UL) /*!< Position of COMPARE0 field. */
#define RTC_INTENSET_COMPARE0_Msk (0x1UL << RTC_INTENSET_COMPARE0_Pos) /*!< Bit mask of COMPARE0 field. */
#define RTC_INTENSET_COMPARE0_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_INTENSET_COMPARE0_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_INTENSET_COMPARE0_Set (0x1UL) /*!< Enable */

/* Bit 1 : Write '1' to enable interrupt for event OVRFLW */
#define RTC_INTENSET_OVRFLW_Pos (1UL) /*!< Position of OVRFLW field. */
#define RTC_INTENSET_OVRFLW_Msk (0x1UL << RTC_INTENSET_OVRFLW_Pos) /*!< Bit mask of OVRFLW field. */
#define RTC_INTENSET_OVRFLW_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_INTENSET_OVRFLW_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_INTENSET_OVRFLW_Set (0x1UL) /*!< Enable */

/* Bit 0 : Write '1' to enable interrupt for event TICK */
#define RTC_INTENSET_TICK_Pos (0UL) /*!< Position of TICK field. */
#define RTC_INTENSET_TICK_Msk (0x1UL << RTC_INTENSET_TICK_Pos) /*!< Bit mask of TICK field. */
#define RTC_INTENSET_TICK_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_INTENSET_TICK_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_INTENSET_TICK_Set (0x1UL) /*!< Enable */

/* Register: RTC_INTENCLR */
/* Description: Disable interrupt */

/* Bit 19 : Write '1' to disable interrupt for event COMPARE[3] */
#define RTC_INTENCLR_COMPARE3_Pos (19UL) /*!< Position of COMPARE3 field. */
#define RTC_INTENCLR_COMPARE3_Msk (0x1UL << RTC_INTENCLR_COMPARE3_Pos) /*!< Bit mask of COMPARE3 field. */
#define RTC_INTENCLR_COMPARE3_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_INTENCLR_COMPARE3_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_INTENCLR_COMPARE3_Clear (0x1UL) /*!< Disable */

/* Bit 18 : Write '1' to disable interrupt for event COMPARE[2] */
#define RTC_INTENCLR_COMPARE2_Pos (18UL) /*!< Position of COMPARE2 field. */
#define RTC_INTENCLR_COMPARE2_Msk (0x1UL << RTC_INTENCLR_COMPARE2_Pos) /*!< Bit mask of COMPARE2 field. */
#define RTC_INTENCLR_COMPARE2_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_INTENCLR_COMPARE2_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_INTENCLR_COMPARE2_Clear (0x1UL) /*!< Disable */

/* Bit 17 : Write '1' to disable interrupt for event COMPARE[1] */
#define RTC_INTENCLR_COMPARE1_Pos (17UL) /*!< Position of COMPARE1 field. */
#define RTC_INTENCLR_COMPARE1_Msk (0x1UL << RTC_INTENCLR_COMPARE1_Pos) /*!< Bit mask of COMPARE1 field. */
#define RTC_INTENCLR_COMPARE1_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_INTENCLR_COMPARE1_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_INTENCLR_COMPARE1_Clear (0x1UL) /*!< Disable */

/* Bit 16 : Write '1' to disable interrupt for event COMPARE[0] */
#define RTC_INTENCLR_COMPARE0_Pos (16UL) /*!< Position of COMPARE0 field. */
#define RTC_INTENCLR_COMPARE0_Msk (0x1UL << RTC_INTENCLR_COMPARE0_Pos) /*!< Bit mask of COMPARE0 field. */
#define RTC_INTENCLR_COMPARE0_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_INTENCLR_COMPARE0_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_INTENCLR_COMPARE0_Clear (0x1UL) /*!< Disable */

/* Bit 1 : Write '1' to disable interrupt for event OVRFLW */
#define RTC_INTENCLR_OVRFLW_Pos (1UL) /*!< Position of OVRFLW field. */
#define RTC_INTENCLR_OVRFLW_Msk (0x1UL << RTC_INTENCLR_OVRFLW_Pos) /*!< Bit mask of OVRFLW field. */
#define RTC_INTENCLR_OVRFLW_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_INTENCLR_OVRFLW_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_INTENCLR_OVRFLW_Clear (0x1UL) /*!< Disable */

/* Bit 0 : Write '1' to disable interrupt for event TICK */
#define RTC_INTENCLR_TICK_Pos (0UL) /*!< Position of TICK field. */
#define RTC_INTENCLR_TICK_Msk (0x1UL << RTC_INTENCLR_TICK_Pos) /*!< Bit mask of TICK field. */
#define RTC_INTENCLR_TICK_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_INTENCLR_TICK_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_INTENCLR_TICK_Clear (0x1UL) /*!< Disable */

/* Register: RTC_EVTEN */
/* Description: Enable or disable event routing */

/* Bit 19 : Enable or disable event routing for event COMPARE[3] */
#define RTC_EVTEN_COMPARE3_Pos (19UL) /*!< Position of COMPARE3 field. */
#define RTC_EVTEN_COMPARE3_Msk (0x1UL << RTC_EVTEN_COMPARE3_Pos) /*!< Bit mask of COMPARE3 field. */
#define RTC_EVTEN_COMPARE3_Disabled (0x0UL) /*!< Disable */
#define RTC_EVTEN_COMPARE3_Enabled (0x1UL) /*!< Enable */

/* Bit 18 : Enable or disable event routing for event COMPARE[2] */
#define RTC_EVTEN_COMPARE2_Pos (18UL) /*!< Position of COMPARE2 field. */
#define RTC_EVTEN_COMPARE2_Msk (0x1UL << RTC_EVTEN_COMPARE2_Pos) /*!< Bit mask of COMPARE2 field. */
#define RTC_EVTEN_COMPARE2_Disabled (0x0UL) /*!< Disable */
#define RTC_EVTEN_COMPARE2_Enabled (0x1UL) /*!< Enable */

/* Bit 17 : Enable or disable event routing for event COMPARE[1] */
#define RTC_EVTEN_COMPARE1_Pos (17UL) /*!< Position of COMPARE1 field. */
#define RTC_EVTEN_COMPARE1_Msk (0x1UL << RTC_EVTEN_COMPARE1_Pos) /*!< Bit mask of COMPARE1 field. */
#define RTC_EVTEN_COMPARE1_Disabled (0x0UL) /*!< Disable */
#define RTC_EVTEN_COMPARE1_Enabled (0x1UL) /*!< Enable */

/* Bit 16 : Enable or disable event routing for event COMPARE[0] */
#define RTC_EVTEN_COMPARE0_Pos (16UL) /*!< Position of COMPARE0 field. */
#define RTC_EVTEN_COMPARE0_Msk (0x1UL << RTC_EVTEN_COMPARE0_Pos) /*!< Bit mask of COMPARE0 field. */
#define RTC_EVTEN_COMPARE0_Disabled (0x0UL) /*!< Disable */
#define RTC_EVTEN_COMPARE0_Enabled (0x1UL) /*!< Enable */

/* Bit 1 : Enable or disable event routing for event OVRFLW */
#define RTC_EVTEN_OVRFLW_Pos (1UL) /*!< Position of OVRFLW field. */
#define RTC_EVTEN_OVRFLW_Msk (0x1UL << RTC_EVTEN_OVRFLW_Pos) /*!< Bit mask of OVRFLW field. */
#define RTC_EVTEN_OVRFLW_Disabled (0x0UL) /*!< Disable */
#define RTC_EVTEN_OVRFLW_Enabled (0x1UL) /*!< Enable */

/* Bit 0 : Enable or disable event routing for event TICK */
#define RTC_EVTEN_TICK_Pos (0UL) /*!< Position of TICK field. */
#define RTC_EVTEN_TICK_Msk (0x1UL << RTC_EVTEN_TICK_Pos) /*!< Bit mask of TICK field. */
#define RTC_EVTEN_TICK_Disabled (0x0UL) /*!< Disable */
#define RTC_EVTEN_TICK_Enabled (0x1UL) /*!< Enable */

/* Register: RTC_EVTENSET */
/* Description: Enable event routing */

/* Bit 19 : Write '1' to enable event routing for event COMPARE[3] */
#define RTC_EVTENSET_COMPARE3_Pos (19UL) /*!< Position of COMPARE3 field. */
#define RTC_EVTENSET_COMPARE3_Msk (0x1UL << RTC_EVTENSET_COMPARE3_Pos) /*!< Bit mask of COMPARE3 field. */
#define RTC_EVTENSET_COMPARE3_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_EVTENSET_COMPARE3_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_EVTENSET_COMPARE3_Set (0x1UL) /*!< Enable */

/* Bit 18 : Write '1' to enable event routing for event COMPARE[2] */
#define RTC_EVTENSET_COMPARE2_Pos (18UL) /*!< Position of COMPARE2 field. */
#define RTC_EVTENSET_COMPARE2_Msk (0x1UL << RTC_EVTENSET_COMPARE2_Pos) /*!< Bit mask of COMPARE2 field. */
#define RTC_EVTENSET_COMPARE2_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_EVTENSET_COMPARE2_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_EVTENSET_COMPARE2_Set (0x1UL) /*!< Enable */

/* Bit 17 : Write '1' to enable event routing for event COMPARE[1] */
#define RTC_EVTENSET_COMPARE1_Pos (17UL) /*!< Position of COMPARE1 field. */
#define RTC_EVTENSET_COMPARE1_Msk (0x1UL << RTC_EVTENSET_COMPARE1_Pos) /*!< Bit mask of COMPARE1 field. */
#define RTC_EVTENSET_COMPARE1_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_EVTENSET_COMPARE1_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_EVTENSET_COMPARE1_Set (0x1UL) /*!< Enable */

/* Bit 16 : Write '1' to enable event routing for event COMPARE[0] */
#define RTC_EVTENSET_COMPARE0_Pos (16UL) /*!< Position of COMPARE0 field. */
#define RTC_EVTENSET_COMPARE0_Msk (0x1UL << RTC_EVTENSET_COMPARE0_Pos) /*!< Bit mask of COMPARE0 field. */
#define RTC_EVTENSET_COMPARE0_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_EVTENSET_COMPARE0_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_EVTENSET_COMPARE0_Set (0x1UL) /*!< Enable */

/* Bit 1 : Write '1' to enable event routing for event OVRFLW */
#define RTC_EVTENSET_OVRFLW_Pos (1UL) /*!< Position of OVRFLW field. */
#define RTC_EVTENSET_OVRFLW_Msk (0x1UL << RTC_EVTENSET_OVRFLW_Pos) /*!< Bit mask of OVRFLW field. */
#define RTC_EVTENSET_OVRFLW_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_EVTENSET_OVRFLW_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_EVTENSET_OVRFLW_Set (0x1UL) /*!< Enable */

/* Bit 0 : Write '1' to enable event routing for event TICK */
#define RTC_EVTENSET_TICK_Pos (0UL) /*!< Position of TICK field. */
#define RTC_EVTENSET_TICK_Msk (0x1UL << RTC_EVTENSET_TICK_Pos) /*!< Bit mask of TICK field. */
#define RTC_EVTENSET_TICK_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_EVTENSET_TICK_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_EVTENSET_TICK_Set (0x1UL) /*!< Enable */

/* Register: RTC_EVTENCLR */
/* Description: Disable event routing */

/* Bit 19 : Write '1' to disable event routing for event COMPARE[3] */
#define RTC_EVTENCLR_COMPARE3_Pos (19UL) /*!< Position of COMPARE3 field. */
#define RTC_EVTENCLR_COMPARE3_Msk (0x1UL << RTC_EVTENCLR_COMPARE3_Pos) /*!< Bit mask of COMPARE3 field. */
#define RTC_EVTENCLR_COMPARE3_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_EVTENCLR_COMPARE3_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_EVTENCLR_COMPARE3_Clear (0x1UL) /*!< Disable */

/* Bit 18 : Write '1' to disable event routing for event COMPARE[2] */
#define RTC_EVTENCLR_COMPARE2_Pos (18UL) /*!< Position of COMPARE2 field. */
#define RTC_EVTENCLR_COMPARE2_Msk (0x1UL << RTC_EVTENCLR_COMPARE2_Pos) /*!< Bit mask of COMPARE2 field. */
#define RTC_EVTENCLR_COMPARE2_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_EVTENCLR_COMPARE2_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_EVTENCLR_COMPARE2_Clear (0x1UL) /*!< Disable */

/* Bit 17 : Write '1' to disable event routing for event COMPARE[1] */
#define RTC_EVTENCLR_COMPARE1_Pos (17UL) /*!< Position of COMPARE1 field. */
#define RTC_EVTENCLR_COMPARE1_Msk (0x1UL << RTC_EVTENCLR_COMPARE1_Pos) /*!< Bit mask of COMPARE1 field. */
#define RTC_EVTENCLR_COMPARE1_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_EVTENCLR_COMPARE1_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_EVTENCLR_COMPARE1_Clear (0x1UL) /*!< Disable */

/* Bit 16 : Write '1' to disable event routing for event COMPARE[0] */
#define RTC_EVTENCLR_COMPARE0_Pos (16UL) /*!< Position of COMPARE0 field. */
#define RTC_EVTENCLR_COMPARE0_Msk (0x1UL << RTC_EVTENCLR_COMPARE0_Pos) /*!< Bit mask of COMPARE0 field. */
#define RTC_EVTENCLR_COMPARE0_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_EVTENCLR_COMPARE0_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_EVTENCLR_COMPARE0_Clear (0x1UL) /*!< Disable */

/* Bit 1 : Write '1' to disable event routing for event OVRFLW */
#define RTC_EVTENCLR_OVRFLW_Pos (1UL) /*!< Position of OVRFLW field. */
#define RTC_EVTENCLR_OVRFLW_Msk (0x1UL << RTC_EVTENCLR_OVRFLW_Pos) /*!< Bit mask of OVRFLW field. */
#define RTC_EVTENCLR_OVRFLW_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_EVTENCLR_OVRFLW_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_EVTENCLR_OVRFLW_Clear (0x1UL) /*!< Disable */

/* Bit 0 : Write '1' to disable event routing for event TICK */
#define RTC_EVTENCLR_TICK_Pos (0UL) /*!< Position of TICK field. */
#define RTC_EVTENCLR_TICK_Msk (0x1UL << RTC_EVTENCLR_TICK_Pos) /*!< Bit mask of TICK field. */
#define RTC_EVTENCLR_TICK_Disabled (0x0UL) /*!< Read: Disabled */
#define RTC_EVTENCLR_TICK_Enabled (0x1UL) /*!< Read: Enabled */
#define RTC_EVTENCLR_TICK_Clear (0x1UL) /*!< Disable */

/* Register: RTC_COUNTER */
/* Description: Current counter value */

/* Bits 23..0 : Counter value */
#define RTC_COUNTER_COUNTER_Pos (0UL) /*!< Position of COUNTER field. */
#define RTC_COUNTER_COUNTER_Msk (0xFFFFFFUL << RTC_COUNTER_COUNTER_Pos) /*!< Bit mask of COUNTER field. */

/* Register: RTC_PRESCALER */
/* Description: 12-bit prescaler for counter frequency (32768 / (PRESCALER + 1)). Must be written when RTC is stopped. */

/* Bits 11..0 : Prescaler value */
#define RTC_PRESCALER_PRESCALER_Pos (0UL) /*!< Position of PRESCALER field. */
#define RTC_PRESCALER_PRESCALER_Msk (0xFFFUL << RTC_PRESCALER_PRESCALER_Pos) /*!< Bit mask of PRESCALER field. */

/* Register: RTC_CC */
/* Description: Description collection: Compare register n */

/* Bits 23..0 : Compare value */
#define RTC_CC_COMPARE_Pos (0UL) /*!< Position of COMPARE field. */
#define RTC_CC_COMPARE_Msk (0xFFFFFFUL << RTC_CC_COMPARE_Pos) /*!< Bit mask of COMPARE field. */


typedef struct {
    unsigned int volatile TASKS_START;
    unsigned int volatile TASKS_STOP;
    unsigned int volatile TASKS_CLEAR;
    unsigned int volatile TASKS_TRIGOVRFLW;
    unsigned int RESERVED[12];
    unsigned int volatile TASKS_CAPTURE[4];
    unsigned int RESERVED1[12];
    unsigned int volatile SUBSCRIBE_START;
    unsigned int volatile SUBSCRIBE_STOP;
    unsigned int volatile SUBSCRIBE_CLEAR;
    unsigned int volatile SUBSCRIBE_TRIGOVRFLW;
    unsigned int RESERVED2[12];
    unsigned int volatile SUBSCRIBE_CAPTURE[4];
    unsigned int RESERVED3[12];
    unsigned int volatile EVENTS_TICK;
    unsigned int volatile EVENTS_OVRFLW;
    unsigned int RESERVED4[14];
    unsigned int volatile EVENTS_COMPARE[4];
    unsigned int RESERVED5[12];
    unsigned int volatile PUBLISH_TICK;
    unsigned int volatile PUBLISH_OVRFLW;
    unsigned int RESERVED6[14];
    unsigned int volatile PUBLISH_COMPARE[4];
    unsigned int RESERVED7[12];
    unsigned int volatile SHORTS;
    unsigned int RESERVED8[63];
    unsigned int volatile INTEN;
    unsigned int volatile INTENSET;
    unsigned int volatile INTENCLR;
    unsigned int RESERVED9[13];
    unsigned int volatile EVTEN;
    unsigned int volatile EVTENSET;
    unsigned int volatile EVTENCLR;
    unsigned int RESERVED10[110];
    unsigned int COUNTER;
    unsigned int volatile PRESCALER;
    unsigned int RESERVED11[13];
    unsigned int volatile CC[4];
} NRF_RTC_Type;

#define NRF_RTC10 (NRF_RTC_Type *)0x50105000

#ifdef __cplusplus
}
#endif

#endif /* NRF54L15 */

#endif /* PPI_SEQ_SPI_SRC_RTC_H__ */
