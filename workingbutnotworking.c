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


#define MASTER_DDCCI_VCP_REQUEST 0x01;

case MASTER_DDCCI_VCP_REQUEST:
  chprintf(chp, "chp request");

  if(ddcRequest[1] == 0xFF) /* invalid request */
  {
    chprintf (chp, "got invalid data for ddc/ci\r\n");
    break;
  }
  retrycap = 5;
  if(ddcci_write_slave (ddcRequest, 5) < 0)
  {
      chprintf(chp, "ddcciwrite failed\r\n");
      break;
  }
  else
  {
    chprintf(chp, "ddcciwrite succeeded\r\n");
    if(ddcci_read_slave(capAnswer) < 0)
    {
      chprintf(chp, "failed reading ddc/ci, retrying\r\n");
      while(retrycap)
      {
        if(ddcci_write_slave (ddcRequest, 5) == 0)
        {
          if(ddcci_read_slave(capAnswer) < 0)
          {
            chprintf(chp, "failed reading ddc/ci, retrying\r\n");
          }
          else break;
        }
        retrycap--;
      }
    }
  }
  /* Now, send this information to the host */
  messageLength = capAnswer[1] & 0x7F;
  if(data == MASTER_DDCCI_ANSWER_REQUEST)
  {
    chprintf(chp, "Ich sollte jetzt schreiben\r\n");
    returncode = ddcci_write_master (capAnswer, messageLength + 3, 0);
    if (returncode < 0) chprintf(chp, "no ack on bytes\r\n");
    else if (returncode > 0)
    {
      chprintf(chp, "ack on checksum\r\n");
    }
    else
    {
      chprintf(chp, "transmission complete\r\n");
    }
  }
break;
