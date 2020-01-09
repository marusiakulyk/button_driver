#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>       // Required for the GPIO functions
#include <linux/kobject.h>    // Using kobjects for the sysfs bindings
#include <linux/kthread.h>    // Using kthreads for the flashing functionality
#include <linux/delay.h>      // Using this header for the msleep() function

#include "gpio_lkm.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Derek Molloy");
MODULE_DESCRIPTION("A simple Linux LED driver LKM for the BBB");
MODULE_VERSION("0.1");

static unsigned int gpioLED = 16;           ///< Default GPIO for the LED is 49
module_param(gpioLED, uint, 0660);          ///< Param desc. S_IRUGO can be read/not changed
MODULE_PARM_DESC(gpioLED, " GPIO LED number (default=49)");     ///< parameter description

static unsigned int gpioBTTN = 19;
module_param(gpioBTTN, uint, 0660);
MODULE_PARM_DESC(gpioBTTN, "GPIO BUTTON number (default=19)");

static char ledName[7] = "ledXXX";          ///< Null terminated default string -- just in case


static int counter = 0;
uint64_t last_interrupt_time = 0;
struct timeval tv ;
struct timeval timeval;
static uint64_t epochMilli;


int intr = -1;

/** @brief A callback function to display the LED mode
 *  @param kobj represents a kernel object device that appears in the sysfs filesystem
 *  @param attr the pointer to the kobj_attribute struct
 *  @param buf the buffer to which to write the number of presses
 *  @return return the number of characters of the mode string successfully displayed
 */


/** @brief A callback function to store the LED mode using the enum above */


/** @brief A callback function to display the LED period */


/** @brief A callback function to store the LED period value */

/** Use these helper macros to define the name and access levels of the kobj_attributes
 *  The kobj_attribute has an attribute attr (name and mode), show and store function pointers
 *  The period variable is associated with the blinkPeriod variable and it is to be exposed
 *  with mode 0660 using the period_show and period_store functions above
 */

///** The ebb_attrs[] is an array of attributes that is used to create the attribute group below.

///** The attribute group uses the attribute array and a name, which is exposed on sysfs -- in this

static ssize_t counter_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
    return sprintf(buf, "%d\n", counter);
}

static ssize_t counter_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count){
    int pcount;
    sscanf(buf, "%d", &pcount);
    counter = pcount;
    return pcount;
}

static struct kobj_attribute counter_attr = __ATTR(counter, 0660, counter_show, counter_store);
static struct attribute *ebb_attrs[] = {&counter_attr.attr, NULL};
static struct attribute_group attr_group = {.name = ledName, .attrs = ebb_attrs};
static struct kobject *ebb_kobj;


/** @brief The LED Flasher main kthread loop
 *
 *  @param arg A void pointer used in order to pass data to the thread
 *  @return returns 0 if successful
 */


/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point. In this example this
 *  function sets up the GPIOs and the IRQ
 *  @return returns 0 if successful
 */



unsigned int millis(void){
    uint64_t timeNow;
    do_gettimeofday(&timeval);
    timeNow = (uint64_t)timeval.tv_sec * (uint64_t)1000 + (uint64_t)(timeval.tv_usec / 1000);

    return (uint8_t)(timeNow - epochMilli);
}

static irqreturn_t irq_handler(int irq, void *arg){
    unsigned long flags;
    unsigned int interrupt_time = millis();

    if (interrupt_time - last_interrupt_time < 200){
        printk(KERN_NOTICE "Ignored Interrupt [%d]\n", irq);
        return IRQ_HANDLED;
    }
    if(counter==10)
        return IRQ_HANDLED;

    counter++;
    last_interrupt_time = interrupt_time;
    local_irq_save(flags);
    printk(KERN_NOTICE "Interrupt [%d] was triggered\n", irq);
    local_irq_restore(flags);

    if(counter==10)
        gpio_set_value(gpioLED, 1);
        io_unable = 1;

    return IRQ_HANDLED;
}

static int __init ebbLED_init(void){
    int result = 0;
    do_gettimeofday(&tv) ;
    epochMilli = (uint64_t)tv.tv_sec *(uint64_t)1000 +
                          (uint64_t)(tv.tv_usec/1000);


    printk(KERN_INFO "EBB LED: Initializing the EBB LED LKM\n");
    sprintf(ledName, "led%d", gpioLED);      // Create the gpio115 name for /sys/ebb/led49

    ebb_kobj = kobject_create_and_add("ebb", kernel_kobj->parent); // kernel_kobj points to /sys/kernel
    if(!ebb_kobj){
        printk(KERN_ALERT "EBB LED: failed to create kobject\n");
        return -ENOMEM;
    }
    // add the attributes to /sys/ebb/ -- for example, /sys/ebb/led49/ledOn
    result = sysfs_create_group(ebb_kobj, &attr_group);
    if(result) {
        printk(KERN_ALERT "EBB LED: failed to create sysfs group\n");
        kobject_put(ebb_kobj);                // clean up -- remove the kobject sysfs entry
        return result;
    }
    gpio_request(gpioLED, "sysfs");          // gpioLED is 49 by default, request it
    gpio_request(gpioBTTN, "sysfs");
    gpio_direction_output(gpioLED, 0);
    gpio_direction_input(gpioBTTN);

    gpio_export(gpioLED, false);  // causes gpio49 to appear in /sys/class/gpio
    // the second argument prevents the direction from being changed
    gpio_export(gpioBTTN, false);

    do_gettimeofday(&tv) ;
    epochMilli=(uint64_t)tv.tv_sec *(uint64_t)1000 +
               (uint64_t)(tv.tv_usec/1000);

    intr = gpio_to_irq(gpioBTTN);
    if (intr < 0) {
        printk(KERN_ALERT "Failed to get interrupt\n");
    } else {
        int request = request_irq(intr, irq_handler, IRQF_TRIGGER_RISING, "bbb_gpio", NULL);
        if (request) {
            printk(KERN_ALERT "EBB LED: failed to create the interrupt\n");
        }
    }

    return result;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit ebbLED_exit(void){
    if (intr > 0) {
        free_irq(intr, NULL);
    }
    kobject_put(ebb_kobj);                   // clean up -- remove the kobject sysfs entry
    gpio_set_value(gpioLED, 0);              // Turn the LED off, indicates device was unloaded
    gpio_unexport(gpioLED);                  // Unexport the Button GPIO
    gpio_free(gpioLED);
    gpio_direction_output(gpioBTTN, 0);
    gpio_unexport(gpioBTTN);
    gpio_free(gpioBTTN);
// Free the LED GPIO
    printk(KERN_INFO "EBB LED: Goodbye from the EBB LED LKM!\n");
}

/// This next calls are  mandatory -- they identify the initialization function
/// and the cleanup function (as above).
module_init(ebbLED_init);
module_exit(ebbLED_exit);


