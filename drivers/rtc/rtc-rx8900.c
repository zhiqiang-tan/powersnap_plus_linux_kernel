//======================================================================
// Driver for the Epson RTC module RX-8900 SA
// 
// Copyright(C) SEIKO EPSON CORPORATION 2014. All rights reserved.
// 
// Derived from RX-8025 driver: 
// Copyright (C) 2009 Wolfgang Grandegger <wg@grandegger.com>
// 
// Copyright (C) 2005 by Digi International Inc.
// All rights reserved.
// 
// Modified by fengjh at rising.com.cn
// <http://lists.lm-sensors.org/mailman/listinfo/lm-sensors>
// 2006.11
// 
// Code cleanup by Sergei Poselenov, <sposelenov@emcraft.com>
// Converted to new style by Wolfgang Grandegger <wg@grandegger.com>
// Alarm and periodic interrupt added by Dmitry Rakhchev <rda@emcraft.com>
//
// 
// This driver software is distributed as is, without any warranty of any kind,
// either express or implied as further specified in the GNU Public License. This
// software may be used and distributed according to the terms of the GNU Public
// License, version 2 as published by the Free Software Foundation.
// See the file COPYING in the main directory of this archive for more details.
//
// You should have received a copy of the GNU General Public License along with
// this program. If not, see <http://www.gnu.org/licenses/>.
//======================================================================

#if 0
#define DEBUG
#include <linux/device.h>
#undef DEBUG
#endif 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/bcd.h>
#include <linux/i2c.h>
#include <linux/list.h>
#include <linux/rtc.h>
#include <linux/of_gpio.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/input.h>
   

// RX-8900 Basic Time and Calendar Register definitions
#define RX8900_BTC_SEC                 0x00
#define RX8900_BTC_MIN                 0x01
#define RX8900_BTC_HOUR                    0x02
#define RX8900_BTC_WEEK                    0x03
#define RX8900_BTC_DAY                 0x04
#define RX8900_BTC_MONTH               0x05
#define RX8900_BTC_YEAR                    0x06
#define RX8900_BTC_RAM                 0x07
#define RX8900_BTC_ALARM_MIN           0x08
#define RX8900_BTC_ALARM_HOUR          0x09
#define RX8900_BTC_ALARM_WEEK_OR_DAY   0x0A
#define RX8900_BTC_TIMER_CNT_0         0x0B
#define RX8900_BTC_TIMER_CNT_1         0x0C
#define RX8900_BTC_EXT                 0x0D
#define RX8900_BTC_FLAG                    0x0E
#define RX8900_BTC_CTRL                    0x0F

// RX-8900 Extension Register 1 definitions
#define RX8900_EXT_SEC                 0x10
#define RX8900_EXT_MIN                 0x11
#define RX8900_EXT_HOUR                    0x12
#define RX8900_EXT_WEEK                    0x13
#define RX8900_EXT_DAY                 0x14
#define RX8900_EXT_MONTH               0x15
#define RX8900_EXT_YEAR                    0x16
#define RX8900_EXT_TEMP                    0x17
#define RX8900_EXT_BACKUP              0x18

#define RX8900_EXT_TIMER_CNT_0         0x1B
#define RX8900_EXT_TIMER_CNT_1         0x1C
#define RX8900_EXT_EXT                 0x1D
#define RX8900_EXT_FLAG                    0x1E
#define RX8900_EXT_CTRL                    0x1F


// Flag RX8900_BTC_EXT Register bit positions
#define RX8900_BTC_EXT_TSEL0       (1 << 0)
#define RX8900_BTC_EXT_TSEL1       (1 << 1)
#define RX8900_BTC_EXT_FSEL0       (1 << 2)
#define RX8900_BTC_EXT_FSEL1       (1 << 3) 
#define RX8900_BTC_EXT_TE          (1 << 4)
#define RX8900_BTC_EXT_USEL            (1 << 5) 
#define RX8900_BTC_EXT_WADA            (1 << 6)
#define RX8900_BTC_EXT_TEST            (1 << 7)

// Flag RX8900_BTC_FLAG Register bit positions
#define RX8900_BTC_FLAG_VDET       (1 << 0)
#define RX8900_BTC_FLAG_VLF        (1 << 1)

#define RX8900_BTC_FLAG_AF             (1 << 3)
#define RX8900_BTC_FLAG_TF             (1 << 4)
#define RX8900_BTC_FLAG_UF             (1 << 5)

// Flag RX8900_BTC_FLAG Register bit positions
#define RX8900_BTC_CTRL_RESET      (1 << 0)


#define RX8900_BTC_CTRL_AIE        (1 << 3)
#define RX8900_BTC_CTRL_TIE        (1 << 4)
#define RX8900_BTC_CTRL_UIE        (1 << 5)
#define RX8900_BTC_CTRL_CSEL0      (1 << 6)
#define RX8900_BTC_CTRL_CSEL1      (1 << 7)


static const struct i2c_device_id rx8900_id[] = {
   { "rx8900", 0 },
   { }
};
MODULE_DEVICE_TABLE(i2c, rx8900_id);

struct rx8900_data {
   struct i2c_client *client;
   struct rtc_device *rtc;
   struct work_struct work;
   u8 ctrlreg;
   unsigned exiting:1;
};

typedef struct {
   u8 number;
   u8 value;
}reg_data;

#define SE_RTC_REG_READ        _IOWR('p', 0x20, reg_data)      
#define SE_RTC_REG_WRITE   _IOW('p',  0x21, reg_data)  

//----------------------------------------------------------------------SE_RTC_REG_READ
// rx8900_read_reg()
// reads a rx8900 register (see Register defines)
// See also rx8900_read_regs() to read multiple registers.
//
//----------------------------------------------------------------------
static int rx8900_read_reg(struct i2c_client *client, u8 number, u8 *value)
{
   int ret = i2c_smbus_read_byte_data(client, number) ;
       
   //check for error
   if (ret < 0) {
       dev_err(&client->dev, "Unable to read register #%d\n", number);
       return ret;
   }

   *value = (u8)ret;
   return 0;
}

//----------------------------------------------------------------------
// rx8900_read_regs()
// reads a specified number of rx8900 registers (see Register defines)
// See also rx8900_read_reg() to read single register.
//
//----------------------------------------------------------------------
static int rx8900_read_regs(struct i2c_client *client, u8 number, u8 length, u8 *values)
{
   int ret = i2c_smbus_read_i2c_block_data(client, number, length, values);

   //check for length error
   if (ret != length) {
       dev_err(&client->dev, "Unable to read registers #%d..#%d\n", number, number + length - 1);
       return ret < 0 ? ret : -EIO;
   }

   return 0;
}

//----------------------------------------------------------------------
// rx8900_write_reg()
// writes a rx8900 register (see Register defines)
// See also rx8900_write_regs() to write multiple registers.
//
//----------------------------------------------------------------------
static int rx8900_write_reg(struct i2c_client *client, u8 number, u8 value)
{
   int ret = i2c_smbus_write_byte_data(client, number, value);

   //check for error
   if (ret)
       dev_err(&client->dev, "Unable to write register #%d\n", number);


   return ret;
}

//----------------------------------------------------------------------
// rx8900_write_regs()
// writes a specified number of rx8900 registers (see Register defines)
// See also rx8900_write_reg() to write a single register.
//
//----------------------------------------------------------------------
static int rx8900_write_regs(struct i2c_client *client, u8 number, u8 length, u8 *values)
{
   int ret = i2c_smbus_write_i2c_block_data(client, number, length, values);

   //check for error
   if (ret)
       dev_err(&client->dev, "Unable to write registers #%d..#%d\n", number, number + length - 1);

   return ret;
}

//----------------------------------------------------------------------
// rx8900_irq()
// irq handler
//
//----------------------------------------------------------------------
static irqreturn_t rx8900_irq(int irq, void *dev_id)
{
   struct i2c_client *client = dev_id;
   struct rx8900_data *rx8900 = i2c_get_clientdata(client);
   disable_irq_nosync(irq);
   schedule_work(&rx8900->work);

   return IRQ_HANDLED;
}

//----------------------------------------------------------------------
// rx8900_work()
//
//----------------------------------------------------------------------
static void rx8900_work(struct work_struct *work)
{
   struct rx8900_data *rx8900 = container_of(work, struct rx8900_data, work);
   struct i2c_client *client = rx8900->client;
   struct mutex *lock = &rx8900->rtc->ops_lock;
   u8 flags;

   mutex_lock(lock);
   
   if (rx8900_read_reg(client, RX8900_BTC_FLAG, &flags))
       goto out;
       
   dev_dbg(&client->dev, "%s REG[%02xh]=>%02xh\n", __func__, RX8900_BTC_FLAG, flags);

   if (flags & RX8900_BTC_FLAG_VLF)
       dev_warn(&client->dev, "Data loss is detected. All registers must be initialized.\n");
       
   if (flags & RX8900_BTC_FLAG_VDET)
       dev_warn(&client->dev, "Temperature compensation stop detected.\n");        

   // fixed-cycle timer
   if (flags & RX8900_BTC_FLAG_TF) { 
       flags &= ~RX8900_BTC_FLAG_TF; 
       local_irq_disable();
       rtc_update_irq(rx8900->rtc, 1, RTC_PF);// | RTC_IRQF);
       local_irq_enable();
       dev_dbg(&client->dev, "%s: fixed-cycle timer function status: %xh\n", __func__, flags);
   }
   
   // alarm function
   if (flags & RX8900_BTC_FLAG_AF) { 
       flags &= ~RX8900_BTC_FLAG_AF; 
       local_irq_disable();
       rtc_update_irq(rx8900->rtc, 1, RTC_AF);// | RTC_IRQF);
       local_irq_enable();
       dev_dbg(&client->dev, "%s: alarm function status: %xh\n", __func__, flags);
   }
   
   // time update function
   if (flags & RX8900_BTC_FLAG_UF) { 
       flags &= ~RX8900_BTC_FLAG_UF; 
       local_irq_disable();
       rtc_update_irq(rx8900->rtc, 1, RTC_UF);// | RTC_IRQF);
       local_irq_enable();
       dev_dbg(&client->dev, "%s: time update function status: %xh\n", __func__, flags);
   }

   // acknowledge IRQ
   rx8900_write_reg(client, RX8900_BTC_FLAG, 0x0f & flags);    

out:
   if (!rx8900->exiting)
       enable_irq(client->irq);

   mutex_unlock(lock);
}

static int rx8900_get_week_day( u8 reg_week_day )
{
   int i, tm_wday = -1;
   
   for ( i=0; i < 7; i++ )
   {
       if ( reg_week_day & 1 )
       {
           tm_wday = i;
           break;
       }
       reg_week_day >>= 1;
   }
   
   return  tm_wday;
}

//----------------------------------------------------------------------
// rx8900_get_time()
// gets the current time from the rx8900 registers
//
//----------------------------------------------------------------------
static int rx8900_get_time(struct device *dev, struct rtc_time *dt)
{
   struct rx8900_data *rx8900 = dev_get_drvdata(dev);
   u8 date[7];
   int err;

   err = rx8900_read_regs(rx8900->client, RX8900_BTC_SEC, 7, date);
   if (err)
       return err;

   dev_dbg(dev, "%s: read 0x%02x 0x%02x "
       "0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", __func__,
       date[0], date[1], date[2], date[3], date[4], date[5], date[6]);

   dt->tm_sec  = bcd2bin(date[RX8900_BTC_SEC] & 0x7f);
   dt->tm_min  = bcd2bin(date[RX8900_BTC_MIN] & 0x7f);
   dt->tm_hour = bcd2bin(date[RX8900_BTC_HOUR] & 0x3f);
   dt->tm_wday = rx8900_get_week_day( date[RX8900_BTC_WEEK] & 0x7f );
   dt->tm_mday = bcd2bin(date[RX8900_BTC_DAY] & 0x3f);
   dt->tm_mon  = bcd2bin(date[RX8900_BTC_MONTH] & 0x1f) - 1;
   dt->tm_year = bcd2bin(date[RX8900_BTC_YEAR]);
   
   if (dt->tm_year < 70)
       dt->tm_year += 100;

   dev_dbg(dev, "%s: date %ds %dm %dh %dwd %dmd %dm %dy\n", __func__,
       dt->tm_sec, dt->tm_min, dt->tm_hour, dt->tm_wday,
       dt->tm_mday, dt->tm_mon, dt->tm_year);

   return rtc_valid_tm(dt);
}

//----------------------------------------------------------------------
// rx8900_set_time()
// Sets the current time in the rx8900 registers
//
//----------------------------------------------------------------------
static int rx8900_set_time(struct device *dev, struct rtc_time *dt)
{
   struct rx8900_data *rx8900 = dev_get_drvdata(dev);
   u8 date[7];
   int ret = 0;
   
   date[RX8900_BTC_SEC]   = bin2bcd(dt->tm_sec);
   date[RX8900_BTC_MIN]   = bin2bcd(dt->tm_min);
   date[RX8900_BTC_HOUR]  = bin2bcd(dt->tm_hour);      
   date[RX8900_BTC_WEEK]  = 1 << (dt->tm_wday);
   date[RX8900_BTC_DAY]   = bin2bcd(dt->tm_mday);
   date[RX8900_BTC_MONTH] = bin2bcd(dt->tm_mon + 1);
   date[RX8900_BTC_YEAR]  = bin2bcd(dt->tm_year % 100);

   dev_dbg(dev, "%s: write 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
       __func__, date[0], date[1], date[2], date[3], date[4], date[5], date[6]);

   ret =  rx8900_write_regs(rx8900->client, RX8900_BTC_SEC, 7, date);

   return ret;
}

//----------------------------------------------------------------------
// rx8900_init_client()
// initializes the rx8900
//
//----------------------------------------------------------------------
static int rx8900_init_client(struct i2c_client *client, int *need_reset)
{
   u8 flags;
   int need_clear = 0;
   int err = 0;

   err = rx8900_read_reg(client, RX8900_BTC_FLAG, &flags);
   if (err)
       goto out;
       
   if ( flags & RX8900_BTC_FLAG_VDET )
   {
       dev_warn(&client->dev, "Temperature compensation is stop detected.\n");
       need_clear = 1;     
   }
   
   if ( flags & RX8900_BTC_FLAG_VLF )
   {
       dev_warn(&client->dev, "Data loss is detected. All registers must be initialized.\n");
       *need_reset = 1;    
       need_clear = 1; 
   }   
   
   if ( flags & RX8900_BTC_FLAG_AF ){
       dev_warn(&client->dev, "Alarm was detected\n");
       need_clear = 1;
   }   
   
   if ( flags & RX8900_BTC_FLAG_TF ){
       dev_warn(&client->dev, "Timer was detected\n");
       need_clear = 1;
   }   
   
   if ( flags & RX8900_BTC_FLAG_UF ){
       dev_warn(&client->dev, "Update was detected\n");
       need_clear = 1;
   }   

   
   if (*need_reset ) {
       //clear ctrl register
       err = rx8900_write_reg(client, RX8900_BTC_CTRL, RX8900_BTC_CTRL_CSEL0);
       if (err)
           goto out;   
       
       //set second update
       err = rx8900_write_reg(client, RX8900_BTC_EXT, 0x00);
       if (err)
           goto out;
           
   }
   
   if (need_clear) {
       //clear flag register
       err = rx8900_write_reg(client, RX8900_BTC_FLAG, 0x00);
       if (err)
           goto out;
       
   }   

out:
   return err;

}

//----------------------------------------------------------------------
// rx8900_get_alarm()
// reads current Alarm 
//
//----------------------------------------------------------------------
static int rx8900_get_alarm(struct device *dev, struct rtc_wkalrm *t)
{
   struct rx8900_data *rx8900 = dev_get_drvdata(dev);
   struct i2c_client *client = rx8900->client;
   u8 alarmvals[3];        //minute, hour, week/day values
   u8 ctrl[2];             //extension, flag
   int err;

   if (client->irq <= 0)
       return -EINVAL;

   //get current minute, hour, week/day alarm values
   err = rx8900_read_regs(client, RX8900_BTC_ALARM_MIN, 3, alarmvals);
   if (err)
       return err;
       
   //get current extension, flag, control values
   err = rx8900_read_regs(client, RX8900_BTC_EXT, 3, ctrl);
   if (err)
       return err;             
   
   dev_dbg(dev, "%s: minutes:0x%02x hours:0x%02x week/day:0x%02x\n",
       __func__, alarmvals[0], alarmvals[1], alarmvals[2]);

   // Hardware alarm precision is 1 minute
   t->time.tm_sec  = 0;
   t->time.tm_min  = bcd2bin(alarmvals[0] & 0x7f);     
   t->time.tm_hour = bcd2bin(alarmvals[1] & 0x3f);     
       
   if ( ctrl[0] & RX8900_BTC_EXT_WADA )
   {   // Day Alarm
       t->time.tm_wday = -1;
       t->time.tm_mday = bcd2bin(alarmvals[2] & 0x3f); 
   }
   else
   {   // Week Day Alarm
       t->time.tm_wday = rx8900_get_week_day( alarmvals[2] & 0x7f );
       t->time.tm_mday = -1;
   }
   
   t->time.tm_mon  = -1;
   t->time.tm_year = -1;

   dev_dbg(dev, "%s: date: %ds %dm %dh %dwd %dmd %dm %dy\n",
       __func__,
       t->time.tm_sec, t->time.tm_min, t->time.tm_hour, t->time.tm_wday,
       t->time.tm_mday, t->time.tm_mon, t->time.tm_year);
   
   //check if INTR is enabled
   t->enabled = !!(rx8900->ctrlreg & RX8900_BTC_CTRL_AIE); 
   //check if flag is triggered 
   t->pending = (ctrl[1] & RX8900_BTC_FLAG_AF) && t->enabled; 
   
   dev_dbg(dev, "%s: t->enabled: %d t->pending: %d\n",
       __func__,
       t->enabled, t->pending);

   return err;
}

//----------------------------------------------------------------------
// rx8900_set_alarm()
// sets Alarm 
//
//----------------------------------------------------------------------
static int rx8900_set_alarm(struct device *dev, struct rtc_wkalrm *t)
{
   struct i2c_client *client = to_i2c_client(dev);
   struct rx8900_data *rx8900 = dev_get_drvdata(dev);
   u8 alarmvals[3];        //minute, hour, day
   u8 ctrl[2];             //ext, flag registers
   int err;
 
   if (client->irq <= 0)
       return -EINVAL;
           
   dev_dbg(dev, "%s: date: %ds %dm %dh %dwd %dmd %dm %dy\n",
       __func__,
       t->time.tm_sec, t->time.tm_min, t->time.tm_hour, t->time.tm_wday,
       t->time.tm_mday, t->time.tm_mon, t->time.tm_year);
           
   //get current flag register
   err = rx8900_read_regs(client, RX8900_BTC_FLAG, 2, ctrl);
   if (err <0)
       return err;

   // Hardware alarm precision is 1 minute
   alarmvals[0] = bin2bcd(t->time.tm_min);
   alarmvals[1] = bin2bcd(t->time.tm_hour);
   
   if ( ctrl[0] & RX8900_BTC_EXT_WADA )
   {   // Day Alarm
       alarmvals[2] = bin2bcd(t->time.tm_mday);
       dev_dbg(dev, "%s: write min:0x%02x hour:0x%02x mday:0x%02x\n", 
       __func__, alarmvals[0], alarmvals[1], alarmvals[2]);
   }
   else
   {   // Week Day Alarm
       alarmvals[2] = 1 << (t->time.tm_wday);
       dev_dbg(dev, "%s: write min:0x%02x hour:0x%02x wday:0x%02x\n", 
       __func__, alarmvals[0], alarmvals[1], alarmvals[2]);
   }
   

   //check interrupt enable and disable
   if (rx8900->ctrlreg & (RX8900_BTC_CTRL_AIE | RX8900_BTC_CTRL_UIE) ) {
       rx8900->ctrlreg &= ~(RX8900_BTC_CTRL_AIE | RX8900_BTC_CTRL_UIE);
       err = rx8900_write_reg(rx8900->client, RX8900_BTC_CTRL, rx8900->ctrlreg);
       if (err)
           return err;     
   }
           
   //write the new minute and hour values
   err = rx8900_write_regs(rx8900->client, RX8900_BTC_ALARM_MIN, 3, alarmvals);
   if (err)
       return err;

   //clear Alarm Flag
   ctrl[1] &= ~RX8900_BTC_FLAG_AF;
   
   err = rx8900_write_reg(rx8900->client, RX8900_BTC_FLAG, ctrl[1]);
   if (err)
       return err;

   //re-enable interrupt if required
   if (t->enabled) {
       //set update interrupt enable
       if ( rx8900->rtc->uie_rtctimer.enabled )
           rx8900->ctrlreg |= RX8900_BTC_CTRL_UIE; 
       //set alarm interrupt enable
       if ( rx8900->rtc->aie_timer.enabled )
           rx8900->ctrlreg |= RX8900_BTC_CTRL_AIE | RX8900_BTC_CTRL_UIE;
       
       err = rx8900_write_reg(rx8900->client, RX8900_BTC_CTRL, rx8900->ctrlreg);
       if (err)
           return err;
   }

   return 0;
}

//----------------------------------------------------------------------
// rx8900_alarm_irq_enable()
// sets enables Alarm IRQ
//
//----------------------------------------------------------------------
static int rx8900_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
   struct i2c_client *client = to_i2c_client(dev);
   struct rx8900_data *rx8900 = dev_get_drvdata(dev);
   u8 flagreg;
   u8 ctrl;
   int err;
   
   //get the current ctrl settings
   ctrl = rx8900->ctrlreg;

   dev_dbg(dev, "%s: %s => 0x%02x\n", __func__, "ctrl", ctrl);
   
   if (enabled)
   {
       // set update interrupt enable
       if ( rx8900->rtc->uie_rtctimer.enabled )
           ctrl |= RX8900_BTC_CTRL_UIE; 
       // set alarm interrupt enable       
       if ( rx8900->rtc->aie_timer.enabled )
           ctrl |= (RX8900_BTC_CTRL_AIE | RX8900_BTC_CTRL_UIE);        
   }
   else
   {
       //clear update interrupt enable
       if ( ! rx8900->rtc->uie_rtctimer.enabled )
           ctrl &= ~RX8900_BTC_CTRL_UIE; 
       //clear alarm interrupt enable
       if ( ! rx8900->rtc->aie_timer.enabled )
       {
           if ( rx8900->rtc->uie_rtctimer.enabled )
               ctrl &= ~RX8900_BTC_CTRL_AIE;  
           else
               ctrl &= ~(RX8900_BTC_CTRL_AIE | RX8900_BTC_CTRL_UIE); 
       }       
   }
   
   dev_dbg(dev, "%s: %s => 0x%02x\n", __func__, "ctrl", ctrl);

   //clear alarm flag
   err = rx8900_read_reg(client, RX8900_BTC_FLAG, &flagreg);
   if (err <0)
       return err;
   flagreg &= ~RX8900_BTC_FLAG_AF;
   err = rx8900_write_reg(client, RX8900_BTC_FLAG, flagreg); 
   if (err)
       return err;
   
   //update the Control register if the setting changed
   if (ctrl != rx8900->ctrlreg) {
       rx8900->ctrlreg = ctrl;
       err = rx8900_write_reg(client, RX8900_BTC_CTRL, rx8900->ctrlreg); 
       if (err)
           return err;
   }
  
   err = rx8900_read_reg(client, RX8900_BTC_CTRL, &ctrl); 
   if (err)
       return err;
   dev_dbg(dev, "%s: REG[0x%02x] => 0x%02x\n", __func__, RX8900_BTC_CTRL, ctrl);
   
   return 0;
}

//---------------------------------------------------------------------------
// rx8900_ioctl()
//
//---------------------------------------------------------------------------
static int rx8900_ioctl(struct device *dev, unsigned int cmd, unsigned long arg)
{
   struct i2c_client *client = to_i2c_client(dev);
   //struct rx8900_data *rx8900 = dev_get_drvdata(dev);
   //struct mutex *lock = &rx8900->rtc->ops_lock;
   int ret = 0;
   int tmp;
   void __user *argp = (void __user *)arg;
   reg_data reg;
   
   dev_dbg(dev, "%s: cmd=%x\n", __func__, cmd);

   switch (cmd) {
       case SE_RTC_REG_READ:
           if (copy_from_user(&reg, argp, sizeof(reg)))
               return -EFAULT;
           if ( reg.number > RX8900_EXT_CTRL )
               return -EFAULT;             
           //mutex_lock(lock);
           ret = rx8900_read_reg(client, reg.number, &reg.value);
           //mutex_unlock(lock);
           if (! ret )             
               return copy_to_user(argp, &reg, sizeof(reg)) ? -EFAULT : 0;
           break;
           
       case SE_RTC_REG_WRITE:
           if (copy_from_user(&reg, argp, sizeof(reg)))
               return -EFAULT;
           if ( reg.number > RX8900_EXT_CTRL )
               return -EFAULT;                 
           //mutex_lock(lock);
           ret = rx8900_write_reg(client, reg.number, reg.value);
           //mutex_unlock(lock);
           break;
   
       case RTC_VL_READ:
           //mutex_lock(lock);
           ret = rx8900_read_reg(client, RX8900_BTC_FLAG, &reg.value);
           //mutex_unlock(lock);
           if (! ret)  
           {
               tmp = !!(reg.value & (RX8900_BTC_FLAG_VLF | RX8900_BTC_FLAG_VDET));
               return copy_to_user(argp, &tmp, sizeof(tmp)) ? -EFAULT : 0;
           }           
           break;          
               
       case RTC_VL_CLR:
           //mutex_lock(lock);
           ret = rx8900_read_reg(client, RX8900_BTC_FLAG, &reg.value);
           if (! ret)
           {
               reg.value &= ~(RX8900_BTC_FLAG_VLF | RX8900_BTC_FLAG_VDET);
               ret = rx8900_write_reg(client, RX8900_BTC_FLAG, reg.value);
           }
           //mutex_unlock(lock);
           break;
           
       default:
           return -ENOIOCTLCMD;            
   }
   
   return ret;
}

static struct rtc_class_ops rx8900_rtc_ops = {
   .read_time = rx8900_get_time,
   .set_time = rx8900_set_time,
   .read_alarm = rx8900_get_alarm,
   .set_alarm = rx8900_set_alarm,
   .alarm_irq_enable = rx8900_alarm_irq_enable,
   .ioctl = rx8900_ioctl,
};

//----------------------------------------------------------------------
// rx8900_probe()
// probe routine for the rx8900 driver
//
//----------------------------------------------------------------------
static int rx8900_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
   struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
   struct rx8900_data *rx8900;
   int err, need_reset = 0;
   
   dev_dbg(&client->dev, "IRQ %d supplied\n", client->irq);
   
   if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA | 
                                           I2C_FUNC_SMBUS_I2C_BLOCK)) {
       dev_err(&adapter->dev, "doesn't support required functionality\n");
       err = -EIO;
       goto errout;
   }
                   
   rx8900 = devm_kzalloc(&client->dev, sizeof(struct rx8900_data), GFP_KERNEL);
   if (!rx8900) {
       dev_err(&adapter->dev, "failed to alloc memory\n");
       err = -ENOMEM;
       goto errout;
   }

   rx8900->client = client;
   i2c_set_clientdata(client, rx8900);

   err = rx8900_init_client(client, &need_reset);
   if (err)
       goto errout;

   if (need_reset) {
       struct rtc_time tm;
       rtc_time_to_tm(0, &tm);     // set to 1970/1/1
       rx8900_set_time(&client->dev, &tm);
       dev_warn(&client->dev, " - time set to 1970/1/1\n");
   }

   rx8900->rtc = rtc_device_register(client->name, &client->dev, 
                                       &rx8900_rtc_ops, THIS_MODULE);
   
   
   if (IS_ERR(rx8900->rtc)) {
       err = PTR_ERR(rx8900->rtc);
       dev_err(&client->dev, "unable to register the class device\n");
       goto errout;
   }

   if (client->irq > 0) {      
       dev_info(&client->dev, "IRQ %d supplied\n", client->irq);
       INIT_WORK(&rx8900->work, rx8900_work);
       err = devm_request_threaded_irq(&client->dev,
                                       client->irq, 
                                       NULL, 
                                       rx8900_irq, 
                                       IRQF_TRIGGER_LOW | IRQF_ONESHOT,
                                       "rx8900", 
                                       client);

       if (err) {
           dev_err(&client->dev, "unable to request IRQ\n");
           goto errout_reg;
       }
   }

   rx8900->rtc->irq_freq = 1;
   rx8900->rtc->max_user_freq = 1;

   return 0;

errout_reg:
   rtc_device_unregister(rx8900->rtc);

errout:
   dev_err(&adapter->dev, "probing for rx8900 failed\n");
   return err;
}

//----------------------------------------------------------------------
// rx8900_remove()
// remove routine for the rx8900 driver
//      
//----------------------------------------------------------------------
static int rx8900_remove(struct i2c_client *client)
{
   struct rx8900_data *rx8900 = i2c_get_clientdata(client);
   struct mutex *lock = &rx8900->rtc->ops_lock;

   if (client->irq > 0) {
       mutex_lock(lock);
       rx8900->exiting = 1;
       mutex_unlock(lock);

       cancel_work_sync(&rx8900->work);
   }

   rtc_device_unregister(rx8900->rtc);

   return 0;
}

static struct i2c_driver rx8900_driver = {
   .driver = {
       .name = "rtc-rx8900",
       .owner = THIS_MODULE,
   },
   .probe      = rx8900_probe,
   .remove     = rx8900_remove,
   .id_table   = rx8900_id,
};

module_i2c_driver(rx8900_driver);

MODULE_AUTHOR("Val Krutov <val.krutov@erd.epson.com>");
MODULE_DESCRIPTION("RX-8900 SA/LC RTC driver");
MODULE_LICENSE("GPL");
