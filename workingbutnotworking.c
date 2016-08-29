static void cmd_ddcci (BaseSequentialStream *chp, int argc, char *argv[])
{

  BBI2C_t i2cdev01;
  uint8_t data;
  uint8_t init = 1;
  //Slave Device for Host - doe sn't need Start afterwards
  BBI2C_Init (&i2cdev01, GPIOC, 10, GPIOC, 11, 50000, BBI2C_MODE_SLAVE);

  for(;;)
  {
    data = BBI2C_Get_Byte (&i2cdev01);
    if (data == 0xA1)
    {
      if (init)
      {
        write_edid (i2cdev01, dummyEDID);
        savedEDID = read_edid ();
        savedEDID = edid_fuzzer_unary (savedEDID);
        init = 0;
      }

      if(write_edid (i2cdev01, savedEDID) != 0)
      {
        chprintf(chp, "Writing EDID to Host failed\r\n");
      }
      else /* EDID successfully sent to host */
      {
        chprintf(chp, "Sent EDID to Host\r\n");
      }

    }
  }

}
