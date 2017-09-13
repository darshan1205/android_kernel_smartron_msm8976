/*
 *drivers/input/touchscreen/ft5x06_ex_fun.c
 *
 *FocalTech IC driver expand function for debug.
 *
 *Copyright (c) 2010  Focal tech Ltd.
 *
 *This software is licensed under the terms of the GNU General Public
 *License version 2, as published by the Free Software Foundation, and
 *may be copied, distributed, and modified under those terms.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 *
 *Note:the error code of EIO is the general error in this file.
 */

#include "focaltech_ex_fun.h"

#include <linux/netdevice.h>
#include <linux/mount.h>
#include <linux/proc_fs.h>


extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);

u8 *I2CDMABuf_va = NULL;
u32 I2CDMABuf_pa;

#define TPD_MAX_POINTS_5        5
#define TPD_MAX_POINTS_2        2
#define AUTO_CLB_NEED           1
#define AUTO_CLB_NONEED         0

#define FT_REG_CHIP_ID                          0xA3
#define FT_REG_FW_VER                           0xA6   //FW  version
#define FT_REG_VENDOR_ID                        0xA8   // TP vendor ID

struct Upgrade_Info fts_updateinfo[] =
{
        {0x55,"FT5x06",TPD_MAX_POINTS_5,AUTO_CLB_NEED,50, 30, 0x79, 0x03, 1, 2000},
        {0x08,"FT5606",TPD_MAX_POINTS_5,AUTO_CLB_NEED,50, 30, 0x79, 0x06, 100, 2000},
        {0x0a,"FT5x16",TPD_MAX_POINTS_5,AUTO_CLB_NEED,50, 30, 0x79, 0x07, 1, 1500},
        {0x05,"FT6208",TPD_MAX_POINTS_2,AUTO_CLB_NONEED,60, 30, 0x79, 0x05, 10, 2000},
        {0x06,"FT6x06",TPD_MAX_POINTS_2,AUTO_CLB_NONEED,100, 30, 0x79, 0x08, 10, 2000},
        {0x55,"FT5x06i",TPD_MAX_POINTS_5,AUTO_CLB_NEED,50, 30, 0x79, 0x03, 1, 2000},
        {0x14,"FT5336",TPD_MAX_POINTS_5,AUTO_CLB_NEED,30, 30, 0x79, 0x11, 10, 2000},
        {0x13,"FT3316",TPD_MAX_POINTS_5,AUTO_CLB_NEED,30, 30, 0x79, 0x11, 10, 2000},
        {0x12,"FT5436i",TPD_MAX_POINTS_5,AUTO_CLB_NEED,30, 30, 0x79, 0x11, 10, 2000},
        {0x11,"FT5336i",TPD_MAX_POINTS_5,AUTO_CLB_NEED,30, 30, 0x79, 0x11, 10, 2000},
};

struct Upgrade_Info fts_updateinfo_curr;
extern struct ft5x06_ts_data *globle_ft5x06_ts_data;
static struct i2c_client* this_client;
int fts_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf,
                        u32 dw_lenth);
int vendor_id_for_tp = 0;
//static unsigned char CTPM_FW_yeji[] = {
//        #include "ft5336_app_yeji.h"
//};
//static unsigned char CTPM_FW_shenyue[] = {
//        #include "ft5336_app_shenyue.h"
//};
//static unsigned char CTPM_FW_shunyu[] = {
//        #include "ft5336_app_shunyu.h"
//};
//static unsigned char CTPM_FW_lansi[] = {
//        #include "ft5336_app_lansi.h"
//};
static unsigned char CTPM_FW_xinli[] = {
        #include "ft5336_app_truly.h"
};

static DEFINE_MUTEX(g_device_mutex);

/*
*fts_i2c_Read-read data and write data by i2c
*@client: handle of i2c
*@writebuf: Data that will be written to the slave
*@writelen: How many bytes to write
*@readbuf: Where to store data read from slave
*@readlen: How many bytes to read
*
*Returns negative errno, else the number of messages executed
*
*
*/
int fts_i2c_Read(struct i2c_client *client, char *writebuf,
                        int writelen, char *readbuf, int readlen)
{
        int ret;

        if (writelen > 0) {
                struct i2c_msg msgs[] = {
                        {
                         .addr = client->addr,
                         .flags = 0,
                         .len = writelen,
                         .buf = writebuf,
                         },
                        {
                         .addr = client->addr,
                         .flags = I2C_M_RD,
                         .len = readlen,
                         .buf = readbuf,
                         },
                };
                ret = i2c_transfer(client->adapter, msgs, 2);
                if (ret < 0)
                        dev_err(&client->dev, "f%s: i2c read error.\n",
                                __func__);
        } else {
                struct i2c_msg msgs[] = {
                        {
                         .addr = client->addr,
                         .flags = I2C_M_RD,
                         .len = readlen,
                         .buf = readbuf,
                         },
                };
                ret = i2c_transfer(client->adapter, msgs, 1);
                if (ret < 0)
                        dev_err(&client->dev, "%s:i2c read error.\n", __func__);
        }

        return ret;
}

/*write data by i2c*/
int fts_i2c_Write(struct i2c_client *client, char *writebuf, int writelen)
{
        int ret;
        struct i2c_msg msg[] = {
                {
                 .addr = client->addr,
                 .flags = 0,
                 .len = writelen,
                 .buf = writebuf,
                 },
        };

        ret = i2c_transfer(client->adapter, msg, 1);
        if (ret < 0)
                dev_err(&client->dev, "%s i2c write error.\n", __func__);

        return ret;
}

int fts_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue)
{
        unsigned char buf[2] = {0};
        buf[0] = regaddr;
        buf[1] = regvalue;

        return fts_i2c_Write(client, buf, sizeof(buf));
}


int fts_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue)
{
        return fts_i2c_Read(client, &regaddr, 1, regvalue, 1);
}

void focaltech_get_upgrade_array(struct i2c_client *client)
{
        u8 chip_id;
        u32 i;

        i2c_smbus_read_i2c_block_data(client,FT_REG_CHIP_ID,1,&chip_id);
        chip_id = 0x14;  
        for(i=0;i<sizeof(fts_updateinfo)/sizeof(struct Upgrade_Info);i++)
        {
                if(chip_id==fts_updateinfo[i].CHIP_ID)
                {
                        memcpy(&fts_updateinfo_curr, &fts_updateinfo[i], sizeof(struct Upgrade_Info));
                        break;
                }
        }

        if(i >= sizeof(fts_updateinfo)/sizeof(struct Upgrade_Info))
        {
                memcpy(&fts_updateinfo_curr, &fts_updateinfo[0], sizeof(struct Upgrade_Info));
        }
}

int fts_ctpm_auto_clb(struct i2c_client *client)
{
        unsigned char uc_temp = 0x00;
        unsigned char i = 0;

        /*start auto CLB */
        msleep(200);

        fts_write_reg(client, 0, FTS_FACTORYMODE_VALUE);
        /*make sure already enter factory mode */
        msleep(100);
        /*write command to start calibration */
        fts_write_reg(client, 2, 0x4);
        msleep(300);
        if ((fts_updateinfo_curr.CHIP_ID==0x11) ||
                (fts_updateinfo_curr.CHIP_ID==0x12) ||
                (fts_updateinfo_curr.CHIP_ID==0x13) ||
                (fts_updateinfo_curr.CHIP_ID==0x14)) //5x36,5x36i
        {
                for(i=0;i<100;i++)
                {
                        fts_read_reg(client, 0x02, &uc_temp);
                        if (0x02 == uc_temp ||
                                0xFF == uc_temp)
                        {
                                /*if 0x02, then auto clb ok, else 0xff, auto clb failure*/
                            break;
                        }
                        msleep(20);
                }
        } else {
                for(i=0;i<100;i++)
                {
                                fts_read_reg(client, 0, &uc_temp);
                        if (0x0 == ((uc_temp&0x70)>>4))  /*return to normal mode, calibration finish*/
                        {
                            break;
                        }
                        msleep(20);
                }
        }
        /*calibration OK*/
        fts_write_reg(client, 0, 0x40);  /*goto factory mode for store*/
        msleep(200);   /*make sure already enter factory mode*/
        fts_write_reg(client, 2, 0x5);  /*store CLB result*/
        msleep(300);
        fts_write_reg(client, 0, FTS_WORKMODE_VALUE);	/*return to normal mode */
        msleep(300);

        /*store CLB result OK */
        return 0;
}

extern int get_tp_vendor_id (void);
/*
upgrade with *.i file
*/
int fts_ctpm_fw_upgrade_with_i_file(struct i2c_client *client)
{
        u8 *pbt_buf = NULL;
        int i_ret;
        int fw_len;
	 unsigned char *CTPM_FW;
	 int vendor_id = get_tp_vendor_id();
	if (vendor_id == 0)
        {
                vendor_id = vendor_id_for_tp;
                
                } 
        if(vendor_id == 0x80) {
//        	CTPM_FW = CTPM_FW_yeji;
//		fw_len = sizeof(CTPM_FW_yeji);
		}
	 else if (vendor_id == 0xa0) {
//		CTPM_FW = CTPM_FW_shenyue;
//		fw_len = sizeof(CTPM_FW_shenyue);
		}
	 else if (vendor_id ==0x5f){
//	 	CTPM_FW = CTPM_FW_shunyu;
//		fw_len = sizeof(CTPM_FW_shunyu);
		}
	else if (vendor_id ==0x6D ){
//	 	CTPM_FW = CTPM_FW_lansi;
//		fw_len = sizeof(CTPM_FW_lansi);
		}
	else if (vendor_id == 0x5A) {//xinli TP
		CTPM_FW = CTPM_FW_xinli;
		fw_len = sizeof(CTPM_FW_xinli);
		}
	 else {//new tp
	 	//CTPM_FW = CTPM_FW_lansi;
		//fw_len = sizeof(CTPM_FW_lansi);
		printk("xxxxxxxxxxxxFAIL.... this is a new tp");
		return -1;
		}
        /*judge the fw that will be upgraded
        * if illegal, then stop upgrade and return.
        */
        if (fw_len < 8 || fw_len > 32 * 1024) {
                dev_err(&client->dev, "%s:FW length error\n", __func__);
                return -EIO;
        }

        if ((CTPM_FW[fw_len - 8] ^ CTPM_FW[fw_len - 6]) == 0xFF
                && (CTPM_FW[fw_len - 7] ^ CTPM_FW[fw_len - 5]) == 0xFF
                && (CTPM_FW[fw_len - 3] ^ CTPM_FW[fw_len - 4]) == 0xFF) {
                /*FW upgrade */
                pbt_buf = CTPM_FW;
                /*call the upgrade function */
                i_ret = fts_ctpm_fw_upgrade(client, pbt_buf, fw_len);
                if (i_ret != 0)
                        dev_err(&client->dev, "%s:upgrade failed. err.\n",
                                        __func__);
                else if(fts_updateinfo_curr.AUTO_CLB==AUTO_CLB_NEED)
                        fts_ctpm_auto_clb(client);	/*start auto CLB */

        } else {
                dev_err(&client->dev, "%s:FW format error\n", __func__);
                return -EBADFD;
        }

        return i_ret;
}


/*update project setting
*only update these settings for COB project, or for some special case
*/
int fts_ctpm_update_project_setting(struct i2c_client *client)
{
        u8 uc_i2c_addr;	/*I2C slave address (7 bit address)*/
        u8 uc_io_voltage;	/*IO Voltage 0---3.3v;	1----1.8v*/
        u8 uc_panel_factory_id;	/*TP panel factory ID*/
        u8 buf[FTS_SETTING_BUF_LEN];
        u8 reg_val[2] = {0};
        u8 auc_i2c_write_buf[10] = {0};
        u32 i = 0;
        int id ;
        int i_ret;

        uc_i2c_addr = client->addr;
        uc_io_voltage = 0x0;
        uc_panel_factory_id = 0x5a;


        /*Step 1:Reset  CTPM
        *write 0xaa to register 0xfc
        */
        fts_write_reg(client, 0xfc, 0xaa);
        msleep(50);

        /*write 0x55 to register 0xfc */
        fts_write_reg(client, 0xfc, 0x55);
        msleep(30);

        /*********Step 2:Enter upgrade mode *****/
        auc_i2c_write_buf[0] = 0x55;
        auc_i2c_write_buf[1] = 0xaa;
        do {
                i++;
                i_ret = fts_i2c_Write(client, auc_i2c_write_buf, 2);
                msleep(5);
        } while (i_ret <= 0 && i < 5);


        /*********Step 3:check READ-ID***********************/
        auc_i2c_write_buf[0] = 0x90;
        auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] =
                        0x00;

        fts_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);

        if (reg_val[0] == 0x79 && reg_val[1] == 0x11)
                dev_dbg(&client->dev, "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
                         reg_val[0], reg_val[1]);
        else
                return -EIO;

        auc_i2c_write_buf[0] = 0xcd;
        fts_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1);
        dev_dbg(&client->dev, "bootloader version = 0x%x\n", reg_val[0]);

        /*--------- read current project setting  ---------- */
        /*set read start address */
        buf[0] = 0x3;
        buf[1] = 0x0;
        buf[2] = 0x7;
        buf[3] = 0xb4;

        fts_i2c_Read(client, buf, 4, buf, FTS_SETTING_BUF_LEN);
        printk( "[FTS] old setting: uc_i2c_addr = 0x%x,\
                        uc_io_voltage = %d, uc_panel_factory_id = 0x%x\n",
                        buf[0], buf[2], buf[4]);
        id = buf[0];

        /********* reset the new FW***********************/
        auc_i2c_write_buf[0] = 0x07;
        fts_i2c_Write(client, auc_i2c_write_buf, 1);

        msleep(200);
        return id;
}
u8 fts_ctpm_get_i_file_ver(void)
{
        u16 ui_sz;
	 unsigned char *CTPM_FW;
	 int vendor_id = get_tp_vendor_id();
	// printk("%c,%c,%s\n",__FILE__,__func__,__LINE__);
	 printk("_____________  vendor_id=%d\n",vendor_id);
         if (vendor_id == 0)
         {
                 vendor_id_for_tp = fts_ctpm_update_project_setting(this_client);
                 vendor_id = vendor_id_for_tp ;
                 printk ("xxxxxxxx vendor_id_for_tp = %x\n",vendor_id_for_tp);
                  }
        if(vendor_id == 0x80) {
//        	CTPM_FW = CTPM_FW_yeji;
//		ui_sz = sizeof(CTPM_FW_yeji);
		printk("xxxxxxxxxxxxyeji tp\n");
			}
	 else if (vendor_id == 0xa0 ) {
//		CTPM_FW = CTPM_FW_shenyue;
//		ui_sz = sizeof(CTPM_FW_shenyue);
		printk("xxxxxxxxxxxxshenyue tp\n");
		      }
	 else if (vendor_id ==0x5f ){
//	 	CTPM_FW = CTPM_FW_shunyu;
//		ui_sz = sizeof(CTPM_FW_shunyu);
		printk("xxxxxxxxxxxxshunyu tp\n");
		       }
	 else if (vendor_id ==0x6D ){
//	 	CTPM_FW = CTPM_FW_lansi;
//		ui_sz = sizeof(CTPM_FW_lansi);
		printk("xxxxxxxxxxxxlansi tp\n");
		}
	 else if (vendor_id == 0x5A) {//xinli
		CTPM_FW = CTPM_FW_xinli;
		ui_sz = sizeof(CTPM_FW_xinli);
		printk("xxxxxxxxxxxxxinli tp\n");
		}
	 else {//new tp
	 	//CTPM_FW = CTPM_FW_yeji;
		//ui_sz = sizeof(CTPM_FW_yeji);
		printk("xxxxxxxxxxxxFAIL.... this is a new tp");
		return 0x00;
		}
        if (ui_sz > 2)
                return CTPM_FW[ui_sz - 2];

        return 0x00;	/*default value */
}

int fts_ctpm_auto_upgrade(struct i2c_client *client)
{
        u8 uc_host_fm_ver = FT_REG_FW_VER;
        u8 uc_tp_fm_ver;
        int i_ret;

        fts_read_reg(client, FT_REG_FW_VER, &uc_tp_fm_ver);
        this_client = client;
        uc_host_fm_ver = fts_ctpm_get_i_file_ver();

        printk("[FTS] uc_tp_fm_ver = 0x%x, uc_host_fm_ver = 0x%x\n",uc_tp_fm_ver, uc_host_fm_ver);

        if (/*the firmware in touch panel maybe corrupted */
                uc_tp_fm_ver == FT_REG_FW_VER ||
                /*the firmware in host flash is new, need upgrade */
                uc_tp_fm_ver < uc_host_fm_ver || (!uc_tp_fm_ver)
            ) {
                msleep(100);
                dev_dbg(&client->dev, "[FTS] uc_tp_fm_ver = 0x%x, uc_host_fm_ver = 0x%x\n",
                                uc_tp_fm_ver, uc_host_fm_ver);
                i_ret = fts_ctpm_fw_upgrade_with_i_file(client);
                if (i_ret == 0)	{
                        msleep(300);
                        uc_host_fm_ver = fts_ctpm_get_i_file_ver();
                        dev_dbg(&client->dev, "[FTS] upgrade to new version 0x%x\n",
                                        uc_host_fm_ver);
                } else {
                        pr_err("[FTS] upgrade failed ret=%d.\n", i_ret);
                        return -EIO;
                }
        }

        return 0;
}

void delay_qt_ms(unsigned long  w_ms)
{
        unsigned long i;
        unsigned long j;

        for (i = 0; i < w_ms; i++)
        {
                for (j = 0; j < 1000; j++)
                {
                         udelay(1);
                }
        }
}

int fts_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf,
                        u32 dw_lenth)
{
        u8 reg_val[2] = {0};
        u32 i = 0;
        u8 is_5336_new_bootloader = 0;
        u8 is_5336_fwsize_30 = 0;
        u32 packet_number;
        u32 j=0;
        u32 temp;
        u32 lenght;
        u8 packet_buf[FTS_PACKET_LENGTH + 6];
        u8 auc_i2c_write_buf[10];
        u8 bt_ecc;
        // struct Upgrade_Info upgradeinfo;

        //fts_get_upgrade_info(&upgradeinfo);

        focaltech_get_upgrade_array(client);

        if(*(pbt_buf+dw_lenth-12) == 30)
        {
                is_5336_fwsize_30 = 1;
        }
        else
        {
                is_5336_fwsize_30 = 0;
        }
        for (i = 0; i < FTS_UPGRADE_LOOP; i++)
        {
                msleep(100);
                printk("[FTS] Step 1:Reset  CTPM\n");
                /*********Step 1:Reset  CTPM *****/
                /*write 0xaa to register 0xfc */
                //if (DEVICE_IC_TYPE == IC_FT6208 || DEVICE_IC_TYPE == IC_FT6x06)
                if(fts_updateinfo_curr.CHIP_ID==0x05 || fts_updateinfo_curr.CHIP_ID==0x06 )
                        fts_write_reg(client, 0xbc, FT_UPGRADE_AA);
                else
                        fts_write_reg(client, 0xfc, FT_UPGRADE_AA);
                msleep(fts_updateinfo_curr.delay_aa);


                /*write 0x55 to register 0xfc */
                //if(DEVICE_IC_TYPE == IC_FT6208 || DEVICE_IC_TYPE == IC_FT6x06)
                if(fts_updateinfo_curr.CHIP_ID==0x05 || fts_updateinfo_curr.CHIP_ID==0x06 )
                        fts_write_reg(client, 0xbc, FT_UPGRADE_55);
                else
                        fts_write_reg(client, 0xfc, FT_UPGRADE_55);
                if(i<=15)
                {
                msleep(fts_updateinfo_curr.delay_55+i*3);
                }
                else
                {
                msleep(fts_updateinfo_curr.delay_55-(i-15)*2);
                }


                /*********Step 2:Enter upgrade mode *****/
                printk("[FTS] Step 2:Enter upgrade mode \n");
                        auc_i2c_write_buf[0] = FT_UPGRADE_55;
                        fts_i2c_Write(client, auc_i2c_write_buf, 1);
                        msleep(5);
                        auc_i2c_write_buf[0] = FT_UPGRADE_AA;
                        fts_i2c_Write(client, auc_i2c_write_buf, 1);

                /*********Step 3:check READ-ID***********************/
                msleep(fts_updateinfo_curr.delay_readid);
                auc_i2c_write_buf[0] = 0x90;
                auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] =0x00;
                fts_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);

                printk("[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0], reg_val[1]);
                if (reg_val[0] == fts_updateinfo_curr.upgrade_id_1
                        && reg_val[1] == fts_updateinfo_curr.upgrade_id_2) {
                        //dev_dbg(&client->dev, "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
                                //reg_val[0], reg_val[1]);
                        printk("[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
                                reg_val[0], reg_val[1]);
                        break;
                } else {
                        dev_err(&client->dev, "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
                                reg_val[0], reg_val[1]);
                }
        }
        if (i >= FTS_UPGRADE_LOOP)
                return -EIO;

        auc_i2c_write_buf[0] = 0xcd;
        fts_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1);
        /*********0705 mshl ********************/
        /*if (reg_val[0] > 4)
                is_5336_new_bootloader = 1;*/

        if (reg_val[0] <= 4)
        {
                is_5336_new_bootloader = BL_VERSION_LZ4 ;
        }
        else if(reg_val[0] == 7)
        {
                is_5336_new_bootloader = BL_VERSION_Z7 ;
        }
        else if(reg_val[0] >= 0x0f && ((fts_updateinfo_curr.CHIP_ID==0x11) ||(fts_updateinfo_curr.CHIP_ID==0x12) ||(fts_updateinfo_curr.CHIP_ID==0x13) ||(fts_updateinfo_curr.CHIP_ID==0x14)))
        {
                is_5336_new_bootloader = BL_VERSION_GZF ;
        }
        else
        {
                is_5336_new_bootloader = BL_VERSION_LZ4 ;
        }


        printk("[FTS] Step 4:erase app and panel paramenter area\n");
        /*Step 4:erase app and panel paramenter area*/
        printk("Step 4:erase app and panel paramenter area\n");
        auc_i2c_write_buf[0] = 0x61;
        fts_i2c_Write(client, auc_i2c_write_buf, 1);	/*erase app area */
        msleep(fts_updateinfo_curr.delay_earse_flash);
        /*erase panel parameter area */
        if(is_5336_fwsize_30)
        {
            auc_i2c_write_buf[0] = 0x63;
            fts_i2c_Write(client, auc_i2c_write_buf, 1);
        }
        msleep(100);

        printk("[FTS] Step 5:write firmware(FW) to ctpm flash\n");
        /*********Step 5:write firmware(FW) to ctpm flash*********/
        bt_ecc = 0;
        printk("Step 5:write firmware(FW) to ctpm flash\n");

        //dw_lenth = dw_lenth - 8;
        if(is_5336_new_bootloader == BL_VERSION_LZ4 || is_5336_new_bootloader == BL_VERSION_Z7 )
        {
                dw_lenth = dw_lenth - 8;
        }
        else if(is_5336_new_bootloader == BL_VERSION_GZF)
        {
              dw_lenth = dw_lenth - 14;
        }
        packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
        packet_buf[0] = 0xbf;
        packet_buf[1] = 0x00;

        for (j = 0; j < packet_number; j++) {
                temp = j * FTS_PACKET_LENGTH;
                packet_buf[2] = (u8) (temp >> 8);
                packet_buf[3] = (u8) temp;
                lenght = FTS_PACKET_LENGTH;
                packet_buf[4] = (u8) (lenght >> 8);
                packet_buf[5] = (u8) lenght;

                for (i = 0; i < FTS_PACKET_LENGTH; i++) {
                        packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
                        bt_ecc ^= packet_buf[6 + i];
                }

                fts_i2c_Write(client, packet_buf, FTS_PACKET_LENGTH + 6);
                msleep(FTS_PACKET_LENGTH / 6 + 1);
                if((((j+1) * FTS_PACKET_LENGTH)%1024)==0)
                printk("write bytes:0x%04x\n", (j+1) * FTS_PACKET_LENGTH);
                //delay_qt_ms(FTS_PACKET_LENGTH / 6 + 1);
        }

        if ((dw_lenth) % FTS_PACKET_LENGTH > 0) {
                temp = packet_number * FTS_PACKET_LENGTH;
                packet_buf[2] = (u8) (temp >> 8);
                packet_buf[3] = (u8) temp;
                temp = (dw_lenth) % FTS_PACKET_LENGTH;
                packet_buf[4] = (u8) (temp >> 8);
                packet_buf[5] = (u8) temp;

                for (i = 0; i < temp; i++) {
                        packet_buf[6 + i] = pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
                        bt_ecc ^= packet_buf[6 + i];
                }

                fts_i2c_Write(client, packet_buf, temp + 6);
                msleep(20);
        }
        /*send the last six byte*/
        if(is_5336_new_bootloader == BL_VERSION_LZ4 || is_5336_new_bootloader == BL_VERSION_Z7 )
        {
                for (i = 0; i<6; i++)
                {
                        if (is_5336_new_bootloader  == BL_VERSION_Z7 )
                        {
                                temp = 0x7bfa + i;
                        }
                        else if(is_5336_new_bootloader == BL_VERSION_LZ4)
                        {
                                temp = 0x6ffa + i;
                        }
                        packet_buf[2] = (u8)(temp>>8);
                        packet_buf[3] = (u8)temp;
                        temp =1;
                        packet_buf[4] = (u8)(temp>>8);
                        packet_buf[5] = (u8)temp;
                        packet_buf[6] = pbt_buf[ dw_lenth + i];
                        bt_ecc ^= packet_buf[6];

                        fts_i2c_Write(client, packet_buf, 7);
                        msleep(20);
                }
        }
        else if(is_5336_new_bootloader == BL_VERSION_GZF)
        {

                for (i = 0; i<12; i++)
                {
                        if (is_5336_fwsize_30)
                        {
                                temp = 0x7ff4 + i;
                        }
                        else
                        {
                                temp = 0x7bf4 + i;
                        }
                        packet_buf[2] = (u8)(temp>>8);
                        packet_buf[3] = (u8)temp;
                        temp =1;
                        packet_buf[4] = (u8)(temp>>8);
                        packet_buf[5] = (u8)temp;
                        packet_buf[6] = pbt_buf[ dw_lenth + i];
                        bt_ecc ^= packet_buf[6];

                        fts_i2c_Write(client, packet_buf, 7);
                        msleep(20);

                }
        }

        printk("[FTS] Step 6: read out checksum\n");
        /*********Step 6: read out checksum***********************/
        /*send the opration head */
        printk("Step 6: read out checksum\n");
        auc_i2c_write_buf[0] = 0xcc;
        fts_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1);
        if (reg_val[0] != bt_ecc) {
                dev_err(&client->dev, "[FTS]--ecc error! FW=%02x bt_ecc=%02x\n",
                                        reg_val[0],
                                        bt_ecc);
                return -EIO;
        }

        printk("[FTS] Step 7: reset the new FW\n");
        /*********Step 7: reset the new FW***********************/
        printk("Step 7: reset the new FW\n");
        auc_i2c_write_buf[0] = 0x07;
        fts_i2c_Write(client, auc_i2c_write_buf, 1);
        msleep(300);	/*make sure CTP startup normally */

        return 0;
}

/*sysfs debug*/

/*
*get firmware size

@firmware_name:firmware name
*note:the firmware default path is sdcard.
	if you want to change the dir, please modify by yourself.
*/
static int fts_GetFirmwareSize(char *firmware_name)
{
        struct file *pfile = NULL;
        struct inode *inode;
        unsigned long magic;
        off_t fsize = 0;
        char filepath[128];
        memset(filepath, 0, sizeof(filepath));

        sprintf(filepath, "%s", firmware_name);

        if (NULL == pfile)
                pfile = filp_open(filepath, O_RDONLY, 0);

        if (IS_ERR(pfile)) {
                pr_err("error occured while opening file %s.\n", filepath);
                return -EIO;
        }

        inode = pfile->f_dentry->d_inode;
        magic = inode->i_sb->s_magic;
        fsize = inode->i_size;
        filp_close(pfile, NULL);
        return fsize;
}



/*
*read firmware buf for .bin file.

@firmware_name: fireware name
@firmware_buf: data buf of fireware

note:the firmware default path is sdcard.
	if you want to change the dir, please modify by yourself.
*/
static int fts_ReadFirmware(char *firmware_name,
			       unsigned char *firmware_buf)
{
        struct file *pfile = NULL;
        struct inode *inode;
        unsigned long magic;
        off_t fsize;
        char filepath[128];
        loff_t pos;
        mm_segment_t old_fs;

        memset(filepath, 0, sizeof(filepath));
        sprintf(filepath, "%s", firmware_name);
        if (NULL == pfile)
                pfile = filp_open(filepath, O_RDONLY, 0);
        if (IS_ERR(pfile)) {
                pr_err("error occured while opening file %s.\n", filepath);
                return -EIO;
        }

        inode = pfile->f_dentry->d_inode;
        magic = inode->i_sb->s_magic;
        fsize = inode->i_size;
        old_fs = get_fs();
        set_fs(KERNEL_DS);
        pos = 0;
        vfs_read(pfile, firmware_buf, fsize, &pos);
        filp_close(pfile, NULL);
        set_fs(old_fs);

        return 0;
}



/*
upgrade with *.bin file
*/

int fts_ctpm_fw_upgrade_with_app_file(struct i2c_client *client,
                                char *firmware_name)
{
        u8 *pbt_buf = NULL;
        int i_ret=0;
        int fwsize = fts_GetFirmwareSize(firmware_name);

        if (fwsize <= 0) {
                dev_err(&client->dev, "%s ERROR:Get firmware size failed\n",
                                        __func__);
                return -EIO;
        }

        if (fwsize < 8 || fwsize > 32 * 1024) {
                dev_dbg(&client->dev, "%s:FW length error\n", __func__);
                return -EIO;
        }


        /*=========FW upgrade========================*/
        pbt_buf = kmalloc(fwsize + 1, GFP_ATOMIC);

        if (fts_ReadFirmware(firmware_name, pbt_buf)) {
                dev_err(&client->dev, "%s() - ERROR: request_firmware failed\n",
                                        __func__);
                kfree(pbt_buf);
                i_ret = -EIO;
                goto err_ret;
        }

        /*call the upgrade function */
        i_ret = fts_ctpm_fw_upgrade(client, pbt_buf, fwsize);
        if (i_ret != 0)
                dev_err(&client->dev, "%s() - ERROR:[FTS] upgrade failed..\n",
                                        __func__);
        else if(fts_updateinfo_curr.AUTO_CLB==AUTO_CLB_NEED)
                fts_ctpm_auto_clb(client);

err_ret:

        kfree(pbt_buf);

        return i_ret;
}

static ssize_t fts_tpfwver_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
        ssize_t num_read_chars = 0;
        u8 fwver = 0;
        struct i2c_client *client = container_of(dev, struct i2c_client, dev);

        mutex_lock(&g_device_mutex);

        if (fts_read_reg(client, FT_REG_FW_VER, &fwver) < 0)
                num_read_chars = snprintf(buf, FTS_PAGE_SIZE,
                                        "get tp fw version fail!\n");
        else
                num_read_chars = snprintf(buf, FTS_PAGE_SIZE, "%02X\n", fwver);

        mutex_unlock(&g_device_mutex);

        return num_read_chars;
}

static ssize_t fts_tpfwver_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count)
{
        /*place holder for future use*/
        return -EPERM;
}



static ssize_t fts_tprwreg_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
        /*place holder for future use*/
        return -EPERM;
}

static ssize_t fts_tprwreg_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count)
{
        struct i2c_client *client = container_of(dev, struct i2c_client, dev);
        ssize_t num_read_chars = 0;
        int retval;
        long unsigned int wmreg = 0;
        u8 regaddr = 0xff, regvalue = 0xff;
        u8 valbuf[5] = {0};

        memset(valbuf, 0, sizeof(valbuf));
        mutex_lock(&g_device_mutex);
        num_read_chars = count - 1;

        if (num_read_chars != 2) {
                if (num_read_chars != 4) {
                        pr_info("please input 2 or 4 character\n");
                        goto error_return;
                }
        }

        memcpy(valbuf, buf, num_read_chars);
        retval = strict_strtoul(valbuf, 16, &wmreg);

        if (0 != retval) {
                dev_err(&client->dev, "%s() - ERROR: Could not convert the "\
                                                "given input to a number." \
                                                "The given input was: \"%s\"\n",
                                                __func__, buf);
                goto error_return;
        }

        if (2 == num_read_chars) {
                /*read register*/
                regaddr = wmreg;
                if (fts_read_reg(client, regaddr, &regvalue) < 0)
                        dev_err(&client->dev, "Could not read the register(0x%02x)\n",
                                                regaddr);
                else
                        pr_info("the register(0x%02x) is 0x%02x\n",
                                        regaddr, regvalue);
        } else {
                regaddr = wmreg >> 8;
                regvalue = wmreg;
                if (fts_write_reg(client, regaddr, regvalue) < 0)
                        dev_err(&client->dev, "Could not write the register(0x%02x)\n",
                                                        regaddr);
                else
                        dev_err(&client->dev, "Write 0x%02x into register(0x%02x) successful\n",
                                                        regvalue, regaddr);
        }

error_return:
        mutex_unlock(&g_device_mutex);

        return count;
}

static ssize_t fts_fwupdate_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
        /* place holder for future use */
        return -EPERM;
}

/*upgrade from *.i*/
static ssize_t fts_fwupdate_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count)
{
        struct fts_ts_data *data = NULL;
        u8 uc_host_fm_ver;
        int i_ret;
        struct i2c_client *client = container_of(dev, struct i2c_client, dev);

        data = (struct fts_ts_data *)i2c_get_clientdata(client);

        mutex_lock(&g_device_mutex);

        //disable_irq(client->irq);
        ///*mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);*/
        i_ret = fts_ctpm_fw_upgrade_with_i_file(client);
        if (i_ret == 0) {
                msleep(300);
                uc_host_fm_ver = fts_ctpm_get_i_file_ver();
                pr_info("%s [FTS] upgrade to new version 0x%x\n", __func__,
                                         uc_host_fm_ver);
        } else
                dev_err(&client->dev, "%s ERROR:[FTS] upgrade failed.\n",
                                        __func__);

        //enable_irq(client->irq);
        ///*mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);*/
        mutex_unlock(&g_device_mutex);

        return count;
}

static ssize_t fts_fwupgradeapp_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
        /*place holder for future use*/
        return -EPERM;
}


/*upgrade from app.bin*/
static ssize_t fts_fwupgradeapp_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count)
{
        ssize_t num_read_chars = 0;
        char fwname[128];
        //struct i2c_client *client = container_of(dev, struct i2c_client, dev);

        memset(fwname, 0, sizeof(fwname));
        sprintf(fwname, "%s", buf);
        fwname[count - 1] = '\0';

        mutex_lock(&g_device_mutex);
        //disable_irq(client->irq);
        ///*mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);*/
#if 0
        if(0==fts_ctpm_fw_upgrade_with_app_file(client, fwname))
        {
                num_read_chars = snprintf(buf, FTS_PAGE_SIZE,
                                        "FTP firmware upgrade success!\n");
        }
        else
        {
                num_read_chars = snprintf(buf, FTS_PAGE_SIZE,
                                        "FTP firmware upgrade fail!\n");
        }
#endif

        //enable_irq(client->irq);
        ///*mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);*/
        mutex_unlock(&g_device_mutex);

        return num_read_chars;
}


/*sysfs */
/*get the fw version
*example:cat ftstpfwver
*/
static DEVICE_ATTR(ftstpfwver, S_IRUGO | S_IWUSR, fts_tpfwver_show,
                        fts_tpfwver_store);

/*upgrade from *.i
*example: echo 1 > ftsfwupdate
*/
static DEVICE_ATTR(ftsfwupdate, S_IRUGO | S_IWUSR, fts_fwupdate_show,
                        fts_fwupdate_store);

/*read and write register
*read example: echo 88 > ftstprwreg ---read register 0x88
*write example:echo 8807 > ftstprwreg ---write 0x07 into register 0x88
*
*note:the number of input must be 2 or 4.if it not enough,please fill in the 0.
*/
static DEVICE_ATTR(ftstprwreg, S_IRUGO | S_IWUSR, fts_tprwreg_show,
                        fts_tprwreg_store);


/*upgrade from app.bin
*example:echo "*_app.bin" > ftsfwupgradeapp
*/
static DEVICE_ATTR(ftsfwupgradeapp, S_IRUGO | S_IWUSR, fts_fwupgradeapp_show,
                        fts_fwupgradeapp_store);


/*add your attr in here*/
static struct attribute *fts_attributes[] = {
        &dev_attr_ftstpfwver.attr,
        &dev_attr_ftsfwupdate.attr,
        &dev_attr_ftstprwreg.attr,
        &dev_attr_ftsfwupgradeapp.attr,
        NULL
};

static struct attribute_group fts_attribute_group = {
        .attrs = fts_attributes
};

/*create sysfs for debug*/
int fts_create_sysfs(struct i2c_client *client)
{
        int err;
      //  I2CDMABuf_va = (u8 *)dma_alloc_coherent(NULL, FTS_DMA_BUF_SIZE, &I2CDMABuf_pa, GFP_KERNEL);

      //  if(!I2CDMABuf_va)
      //  {
      //          dev_dbg(&client->dev,"%s Allocate DMA I2C Buffer failed!\n",__func__);
       //         return -EIO;
      //  }
       // printk("FTP: I2CDMABuf_pa=%p,val=%x val2=%p\n",&I2CDMABuf_pa,I2CDMABuf_pa,(unsigned char *)I2CDMABuf_pa);
        err = sysfs_create_group(&client->dev.kobj, &fts_attribute_group);
        if (0 != err) {
                dev_err(&client->dev,
                                         "%s() - ERROR: sysfs_create_group() failed.\n",
                                         __func__);
                sysfs_remove_group(&client->dev.kobj, &fts_attribute_group);
                return -EIO;
        } else {
                mutex_init(&g_device_mutex);
                pr_info("ft6x06:%s() - sysfs_create_group() succeeded.\n",
                                __func__);
        }
        return err;
}

void fts_release_sysfs(struct i2c_client *client)
{
        sysfs_remove_group(&client->dev.kobj, &fts_attribute_group);
        mutex_destroy(&g_device_mutex);
        if(I2CDMABuf_va)
        {
                dma_free_coherent(NULL, FTS_DMA_BUF_SIZE, I2CDMABuf_va, I2CDMABuf_pa);
                I2CDMABuf_va = NULL;
                I2CDMABuf_pa = 0;
        }

}
/*create apk debug channel*/
#if 0
#define PROC_UPGRADE			0
#define PROC_READ_REGISTER		1
#define PROC_WRITE_REGISTER	2
#define PROC_RAWDATA			3
#define PROC_AUTOCLB			4

#define PROC_NAME	"ft5x0x-debug"
static unsigned char proc_operate_mode = PROC_RAWDATA;
static struct proc_dir_entry *ft5x0x_proc_entry = NULL;
/*interface of write proc*/
static int ft5x0x_debug_write(struct file *filp,
        const char __user *buff, unsigned long len, void *data)
{
        unsigned char writebuf[FTS_PACKET_LENGTH];
        int buflen = len;
        int writelen = 0;
        int ret = 0;
        struct i2c_client *client = (struct i2c_client *)ft5x0x_proc_entry->data;
        
        if (copy_from_user(&writebuf, buff, buflen)) {
                dev_err(&client->dev, "%s:copy from user error\n", __func__);
                return -EFAULT;
        }
        proc_operate_mode = writebuf[0];

        switch (proc_operate_mode) {
        case PROC_UPGRADE:
                {
                        char upgrade_file_path[128];
                        memset(upgrade_file_path, 0, sizeof(upgrade_file_path));
                        sprintf(upgrade_file_path, "%s", writebuf + 1);
                        upgrade_file_path[buflen-1] = '\0';
                        printk("%s\n", upgrade_file_path);
                        disable_irq(client->irq);

                        ret = fts_ctpm_fw_upgrade_with_app_file(client, upgrade_file_path);

                        enable_irq(client->irq);
                        if (ret < 0) {
                                dev_err(&client->dev, "%s:upgrade failed.\n", __func__);
                                return ret;
                        }
                }
                break;
        case PROC_READ_REGISTER:
                writelen = 1;
                printk("%s:register addr=0x%02x\n", __func__, writebuf[1]);
                ret = fts_i2c_Write(client, writebuf + 1, writelen);
                if (ret < 0) {
                        dev_err(&client->dev, "%s:write iic error\n", __func__);
                        return ret;
                }
                break;
        case PROC_WRITE_REGISTER:
                writelen = 2;
                ret = fts_i2c_Write(client, writebuf + 1, writelen);
                if (ret < 0) {
                        dev_err(&client->dev, "%s:write iic error\n", __func__);
                        return ret;
                }
                break;
        case PROC_RAWDATA:
                break;
        case PROC_AUTOCLB:
                fts_ctpm_auto_clb(client);
                break;

        default:
                break;
}


return len;
}

/*interface of read proc*/
static int ft5x0x_debug_read( char *page, char **start,
        off_t off, int count, int *eof, void *data )
{
        int ret = 0;
        unsigned char buf[FTS_PAGE_SIZE];
        int num_read_chars = 0;
        int readlen = 0;
        u8 regvalue = 0x00, regaddr = 0x00;
        struct i2c_client *client = (struct i2c_client *)ft5x0x_proc_entry->data;
        
        switch (proc_operate_mode) {
        case PROC_UPGRADE:
                /*after calling ft5x0x_debug_write to upgrade*/
                regaddr = 0xA6;
                ret = fts_read_reg(client, regaddr, &regvalue);
                if (ret < 0)
                        num_read_chars = sprintf(buf, "%s", "get fw version failed.\n");
                else
                        num_read_chars = sprintf(buf, "current fw version:0x%02x\n", regvalue);
                break;
        case PROC_READ_REGISTER:
                readlen = 1;
                ret = fts_i2c_Read(client, NULL, 0, buf, readlen);
                if (ret < 0) {
                        dev_err(&client->dev, "%s:read iic error\n", __func__);
                        return ret;
                } else
                        printk("%s:value=0x%02x\n", __func__, buf[0]);
                num_read_chars = 1;
                break;
        case PROC_RAWDATA:
                break;
        default:
                break;
        }

        memcpy(page, buf, num_read_chars);

        return num_read_chars;
}
int ft5x0x_create_apk_debug_channel(struct i2c_client * client)
{
        ft5x0x_proc_entry = create_proc_entry(PROC_NAME, 0777, NULL);
        if (NULL == ft5x0x_proc_entry) {
                dev_err(&client->dev, "Couldn't create proc entry!\n");
                return -ENOMEM;
        } else {
                dev_info(&client->dev, "Create proc entry success!\n");
                ft5x0x_proc_entry->data = client;
                ft5x0x_proc_entry->write_proc = ft5x0x_debug_write;
                ft5x0x_proc_entry->read_proc = ft5x0x_debug_read;
        }
        return 0;
}

void ft5x0x_release_apk_debug_channel(void)
{
        if (ft5x0x_proc_entry)
                remove_proc_entry(PROC_NAME, NULL);
}
#endif
