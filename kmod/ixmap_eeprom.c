#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/mii.h>
#include <linux/vmalloc.h>
#include <asm/io.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>

#include "ixmap_main.h"
#include "ixmap_type.h"
#include "ixmap_common.h"
#include "ixmap_eeprom.h"

static int32_t ixmap_acquire_eeprom(struct ixmap_hw *hw);
static int32_t ixmap_ready_eeprom(struct ixmap_hw *hw);
static void ixmap_release_eeprom(struct ixmap_hw *hw);
static void ixmap_standby_eeprom(struct ixmap_hw *hw);
static void ixmap_shift_out_eeprom_bits(struct ixmap_hw *hw,
	uint16_t data, uint16_t count);
static uint16_t ixmap_shift_in_eeprom_bits(struct ixmap_hw *hw,
	uint16_t count);
static void ixmap_raise_eeprom_clk(struct ixmap_hw *hw, uint32_t *eec);
static void ixmap_lower_eeprom_clk(struct ixmap_hw *hw, uint32_t *eec);
static int32_t ixmap_get_eeprom_semaphore(struct ixmap_hw *hw);
static void ixmap_release_eeprom_semaphore(struct ixmap_hw *hw);
static int32_t ixmap_read_eeprom_buffer_bit_bang(struct ixmap_hw *hw,
	uint16_t offset, uint16_t words, uint16_t *data);

int32_t ixmap_acquire_swfw_sync(struct ixmap_hw *hw, uint16_t mask)
{
	uint32_t gssr = 0;
	uint32_t swmask = mask;
	uint32_t fwmask = mask << 5;
	uint32_t timeout = 200;
	uint32_t i;

	for (i = 0; i < timeout; i++) {
		/*
		 * SW NVM semaphore bit is used for access to all
		 * SW_FW_SYNC bits (not just NVM)
		 */
		if (ixmap_get_eeprom_semaphore(hw))
			return IXGBE_ERR_SWFW_SYNC;

		gssr = IXGBE_READ_REG(hw, IXGBE_GSSR);
		if (!(gssr & (fwmask | swmask))) {
			gssr |= swmask;
			IXGBE_WRITE_REG(hw, IXGBE_GSSR, gssr);
			ixmap_release_eeprom_semaphore(hw);
			return 0;
		} else {
			/* Resource is currently in use by FW or SW */
			ixmap_release_eeprom_semaphore(hw);
			msleep(5);
		}
	}

	/* If time expired clear the bits holding the lock and retry */
	if (gssr & (fwmask | swmask))
		ixmap_release_swfw_sync(hw, gssr & (fwmask | swmask));

	msleep(5);
	return IXGBE_ERR_SWFW_SYNC;
}

void ixmap_release_swfw_sync(struct ixmap_hw *hw, uint16_t mask)
{
	uint32_t gssr;
	uint32_t swmask = mask;

	ixmap_get_eeprom_semaphore(hw);

	gssr = IXGBE_READ_REG(hw, IXGBE_GSSR);
	gssr &= ~swmask;
	IXGBE_WRITE_REG(hw, IXGBE_GSSR, gssr);

	ixmap_release_eeprom_semaphore(hw);
}

int32_t ixmap_read_pba_string(struct ixmap_hw *hw,
	uint8_t *pba_num, uint32_t pba_num_size)
{
	int32_t ret_val;
	uint16_t data;
	uint16_t pba_ptr;
	uint16_t offset;
	uint16_t length;

	if (pba_num == NULL) {
		return IXGBE_ERR_INVALID_ARGUMENT;
	}

	ret_val = hw->eeprom.ops.read(hw, IXGBE_PBANUM0_PTR, &data);
	if (ret_val) {
		return ret_val;
	}

	ret_val = hw->eeprom.ops.read(hw, IXGBE_PBANUM1_PTR, &pba_ptr);
	if (ret_val) {
		return ret_val;
	}

	/*
	 * if data is not ptr guard the PBA must be in legacy format which
	 * means pba_ptr is actually our second data word for the PBA number
	 * and we can decode it into an ascii string
	 */
	if (data != IXGBE_PBANUM_PTR_GUARD) {
		/* we will need 11 characters to store the PBA */
		if (pba_num_size < 11) {
			return IXGBE_ERR_NO_SPACE;
		}

		/* extract hex string from data and pba_ptr */
		pba_num[0] = (data >> 12) & 0xF;
		pba_num[1] = (data >> 8) & 0xF;
		pba_num[2] = (data >> 4) & 0xF;
		pba_num[3] = data & 0xF;
		pba_num[4] = (pba_ptr >> 12) & 0xF;
		pba_num[5] = (pba_ptr >> 8) & 0xF;
		pba_num[6] = '-';
		pba_num[7] = 0;
		pba_num[8] = (pba_ptr >> 4) & 0xF;
		pba_num[9] = pba_ptr & 0xF;

		/* put a null character on the end of our string */
		pba_num[10] = '\0';

		/* switch all the data but the '-' to hex char */
		for (offset = 0; offset < 10; offset++) {
			if (pba_num[offset] < 0xA)
				pba_num[offset] += '0';
			else if (pba_num[offset] < 0x10)
				pba_num[offset] += 'A' - 0xA;
		}

		return 0;
	}

	ret_val = hw->eeprom.ops.read(hw, pba_ptr, &length);
	if (ret_val) {
		return ret_val;
	}

	if (length == 0xFFFF || length == 0) {
		return IXGBE_ERR_PBA_SECTION;
	}

	/* check if pba_num buffer is big enough */
	if (pba_num_size  < (((uint32_t)length * 2) - 1)) {
		return IXGBE_ERR_NO_SPACE;
	}

	/* trim pba length from start of string */
	pba_ptr++;
	length--;

	for (offset = 0; offset < length; offset++) {
		ret_val = hw->eeprom.ops.read(hw, pba_ptr + offset, &data);
		if (ret_val) {
			return ret_val;
		}
		pba_num[offset * 2] = (uint8_t)(data >> 8);
		pba_num[(offset * 2) + 1] = (uint8_t)(data & 0xFF);
	}
	pba_num[offset * 2] = '\0';

	return 0;
}

int32_t ixmap_validate_eeprom_checksum(struct ixmap_hw *hw,
	uint16_t *checksum_val)
{
	int32_t status;
	uint16_t checksum;
	uint16_t read_checksum = 0;

	/*
	 * Read the first word from the EEPROM. If this times out or fails, do
	 * not continue or we could be in for a very long wait while every
	 * EEPROM read fails
	 */
	status = hw->eeprom.ops.read(hw, 0, &checksum);

	if (status == 0) {
		checksum = hw->eeprom.ops.calc_checksum(hw);

		hw->eeprom.ops.read(hw, IXGBE_EEPROM_CHECKSUM, &read_checksum);

		/*
		 * Verify read checksum from EEPROM is the same as
		 * calculated checksum
		 */
		if (read_checksum != checksum)
			status = IXGBE_ERR_EEPROM_CHECKSUM;

		/* If the user cares, return the calculated checksum */
		if (checksum_val)
			*checksum_val = checksum;
	}

	return status;
}

int32_t ixmap_read_eerd(struct ixmap_hw *hw,
	uint16_t offset, uint16_t *data)
{
	return ixmap_read_eerd_buffer(hw, offset, 1, data);
}

int32_t ixmap_read_eerd_buffer(struct ixmap_hw *hw,
	uint16_t offset, uint16_t words, uint16_t *data)
{
	uint32_t eerd;
	int32_t status = 0;
	uint32_t i;

	hw->eeprom.ops.init_params(hw);

	if (words == 0) {
		status = IXGBE_ERR_INVALID_ARGUMENT;
		goto out;
	}

	if (offset >= hw->eeprom.word_size) {
		status = IXGBE_ERR_EEPROM;
		goto out;
	}

	for (i = 0; i < words; i++) {
		eerd = ((offset + i) << IXGBE_EEPROM_RW_ADDR_SHIFT) |
		       IXGBE_EEPROM_RW_REG_START;

		IXGBE_WRITE_REG(hw, IXGBE_EERD, eerd);
		status = ixmap_poll_eerd_eewr_done(hw, IXGBE_NVM_POLL_READ);

		if (status == 0) {
			data[i] = (IXGBE_READ_REG(hw, IXGBE_EERD) >>
				   IXGBE_EEPROM_RW_REG_DATA);
		} else {
			goto out;
		}
	}
out:
	return status;
}

int32_t ixmap_read_eeprom_bit_bang(struct ixmap_hw *hw,
	uint16_t offset, uint16_t *data)
{
	int32_t status;

	hw->eeprom.ops.init_params(hw);

	if (offset >= hw->eeprom.word_size) {
		status = IXGBE_ERR_EEPROM;
		goto out;
	}

	status = ixmap_read_eeprom_buffer_bit_bang(hw, offset, 1, data);

out:
	return status;
}

static int32_t ixmap_read_eeprom_buffer_bit_bang(struct ixmap_hw *hw,
	uint16_t offset, uint16_t words, uint16_t *data)
{
	int32_t status;
	uint16_t word_in;
	uint8_t read_opcode = IXGBE_EEPROM_READ_OPCODE_SPI;
	uint16_t i;

	/* Prepare the EEPROM for reading  */
	status = ixmap_acquire_eeprom(hw);

	if (status == 0) {
		if (ixmap_ready_eeprom(hw) != 0) {
			ixmap_release_eeprom(hw);
			status = IXGBE_ERR_EEPROM;
		}
	}

	if (status == 0) {
		for (i = 0; i < words; i++) {
			ixmap_standby_eeprom(hw);
			/*
			 * Some SPI eeproms use the 8th address bit embedded
			 * in the opcode
			 */
			if ((hw->eeprom.address_bits == 8) &&
			    ((offset + i) >= 128))
				read_opcode |= IXGBE_EEPROM_A8_OPCODE_SPI;

			/* Send the READ command (opcode + addr) */
			ixmap_shift_out_eeprom_bits(hw, read_opcode,
						    IXGBE_EEPROM_OPCODE_BITS);
			ixmap_shift_out_eeprom_bits(hw, (uint16_t)((offset + i) * 2),
						    hw->eeprom.address_bits);

			/* Read the data. */
			word_in = ixmap_shift_in_eeprom_bits(hw, 16);
			data[i] = (word_in >> 8) | (word_in << 8);
		}

		/* End this read operation */
		ixmap_release_eeprom(hw);
	}

	return status;
}

int32_t ixmap_poll_eerd_eewr_done(struct ixmap_hw *hw, uint32_t ee_reg)
{
	uint32_t i;
	uint32_t reg;
	int32_t status = IXGBE_ERR_EEPROM;

	for (i = 0; i < IXGBE_EERD_EEWR_ATTEMPTS; i++) {
		if (ee_reg == IXGBE_NVM_POLL_READ)
			reg = IXGBE_READ_REG(hw, IXGBE_EERD);
		else
			reg = IXGBE_READ_REG(hw, IXGBE_EEWR);

		if (reg & IXGBE_EEPROM_RW_REG_DONE) {
			status = 0;
			break;
		}
		udelay(5);
	}

	return status;
}

static int32_t ixmap_acquire_eeprom(struct ixmap_hw *hw)
{
	int32_t status = 0;
	uint32_t eec;
	uint32_t i;

	if (hw->mac.ops.acquire_swfw_sync(hw, IXGBE_GSSR_EEP_SM)
	    != 0)
		status = IXGBE_ERR_SWFW_SYNC;

	if (status == 0) {
		eec = IXGBE_READ_REG(hw, IXGBE_EEC);

		/* Request EEPROM Access */
		eec |= IXGBE_EEC_REQ;
		IXGBE_WRITE_REG(hw, IXGBE_EEC, eec);

		for (i = 0; i < IXGBE_EEPROM_GRANT_ATTEMPTS; i++) {
			eec = IXGBE_READ_REG(hw, IXGBE_EEC);
			if (eec & IXGBE_EEC_GNT)
				break;
			udelay(5);
		}

		/* Release if grant not acquired */
		if (!(eec & IXGBE_EEC_GNT)) {
			eec &= ~IXGBE_EEC_REQ;
			IXGBE_WRITE_REG(hw, IXGBE_EEC, eec);

			hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_EEP_SM);
			status = IXGBE_ERR_EEPROM;
		}

		/* Setup EEPROM for Read/Write */
		if (status == 0) {
			/* Clear CS and SK */
			eec &= ~(IXGBE_EEC_CS | IXGBE_EEC_SK);
			IXGBE_WRITE_REG(hw, IXGBE_EEC, eec);
			IXGBE_WRITE_FLUSH(hw);
			udelay(1);
		}
	}
	return status;
}

static int32_t ixmap_ready_eeprom(struct ixmap_hw *hw)
{
	int32_t status = 0;
	uint16_t i;
	uint8_t spi_stat_reg;

	/*
	 * Read "Status Register" repeatedly until the LSB is cleared.  The
	 * EEPROM will signal that the command has been completed by clearing
	 * bit 0 of the internal status register.  If it's not cleared within
	 * 5 milliseconds, then error out.
	 */
	for (i = 0; i < IXGBE_EEPROM_MAX_RETRY_SPI; i += 5) {
		ixmap_shift_out_eeprom_bits(hw, IXGBE_EEPROM_RDSR_OPCODE_SPI,
					    IXGBE_EEPROM_OPCODE_BITS);
		spi_stat_reg = (uint8_t)ixmap_shift_in_eeprom_bits(hw, 8);
		if (!(spi_stat_reg & IXGBE_EEPROM_STATUS_RDY_SPI))
			break;

		udelay(5);
		ixmap_standby_eeprom(hw);
	};

	/*
	 * On some parts, SPI write time could vary from 0-20mSec on 3.3V
	 * devices (and only 0-5mSec on 5V devices)
	 */
	if (i >= IXGBE_EEPROM_MAX_RETRY_SPI) {
		status = IXGBE_ERR_EEPROM;
	}

	return status;
}

static void ixmap_release_eeprom(struct ixmap_hw *hw)
{
	uint32_t eec;

	eec = IXGBE_READ_REG(hw, IXGBE_EEC);

	eec |= IXGBE_EEC_CS;  /* Pull CS high */
	eec &= ~IXGBE_EEC_SK; /* Lower SCK */

	IXGBE_WRITE_REG(hw, IXGBE_EEC, eec);
	IXGBE_WRITE_FLUSH(hw);

	udelay(1);

	/* Stop requesting EEPROM access */
	eec &= ~IXGBE_EEC_REQ;
	IXGBE_WRITE_REG(hw, IXGBE_EEC, eec);

	hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_EEP_SM);

	/* Delay before attempt to obtain semaphore again to allow FW access */
	msleep(hw->eeprom.semaphore_delay);
}

static void ixmap_standby_eeprom(struct ixmap_hw *hw)
{
	uint32_t eec;

	eec = IXGBE_READ_REG(hw, IXGBE_EEC);

	/* Toggle CS to flush commands */
	eec |= IXGBE_EEC_CS;
	IXGBE_WRITE_REG(hw, IXGBE_EEC, eec);
	IXGBE_WRITE_FLUSH(hw);
	udelay(1);
	eec &= ~IXGBE_EEC_CS;
	IXGBE_WRITE_REG(hw, IXGBE_EEC, eec);
	IXGBE_WRITE_FLUSH(hw);
	udelay(1);
}

static void ixmap_shift_out_eeprom_bits(struct ixmap_hw *hw,
	uint16_t data, uint16_t count)
{
	uint32_t eec;
	uint32_t mask;
	uint32_t i;

	eec = IXGBE_READ_REG(hw, IXGBE_EEC);

	/*
	 * Mask is used to shift "count" bits of "data" out to the EEPROM
	 * one bit at a time.  Determine the starting bit based on count
	 */
	mask = 0x01 << (count - 1);

	for (i = 0; i < count; i++) {
		/*
		 * A "1" is shifted out to the EEPROM by setting bit "DI" to a
		 * "1", and then raising and then lowering the clock (the SK
		 * bit controls the clock input to the EEPROM).  A "0" is
		 * shifted out to the EEPROM by setting "DI" to "0" and then
		 * raising and then lowering the clock.
		 */
		if (data & mask)
			eec |= IXGBE_EEC_DI;
		else
			eec &= ~IXGBE_EEC_DI;

		IXGBE_WRITE_REG(hw, IXGBE_EEC, eec);
		IXGBE_WRITE_FLUSH(hw);

		udelay(1);

		ixmap_raise_eeprom_clk(hw, &eec);
		ixmap_lower_eeprom_clk(hw, &eec);

		/*
		 * Shift mask to signify next bit of data to shift in to the
		 * EEPROM
		 */
		mask = mask >> 1;
	};

	/* We leave the "DI" bit set to "0" when we leave this routine. */
	eec &= ~IXGBE_EEC_DI;
	IXGBE_WRITE_REG(hw, IXGBE_EEC, eec);
	IXGBE_WRITE_FLUSH(hw);
}

static uint16_t ixmap_shift_in_eeprom_bits(struct ixmap_hw *hw,
	uint16_t count)
{
	uint32_t eec;
	uint32_t i;
	uint16_t data = 0;

	/*
	 * In order to read a register from the EEPROM, we need to shift
	 * 'count' bits in from the EEPROM. Bits are "shifted in" by raising
	 * the clock input to the EEPROM (setting the SK bit), and then reading
	 * the value of the "DO" bit.  During this "shifting in" process the
	 * "DI" bit should always be clear.
	 */
	eec = IXGBE_READ_REG(hw, IXGBE_EEC);

	eec &= ~(IXGBE_EEC_DO | IXGBE_EEC_DI);

	for (i = 0; i < count; i++) {
		data = data << 1;
		ixmap_raise_eeprom_clk(hw, &eec);

		eec = IXGBE_READ_REG(hw, IXGBE_EEC);

		eec &= ~(IXGBE_EEC_DI);
		if (eec & IXGBE_EEC_DO)
			data |= 1;

		ixmap_lower_eeprom_clk(hw, &eec);
	}

	return data;
}

static void ixmap_raise_eeprom_clk(struct ixmap_hw *hw, uint32_t *eec)
{
	/*
	 * Raise the clock input to the EEPROM
	 * (setting the SK bit), then delay
	 */
	*eec = *eec | IXGBE_EEC_SK;
	IXGBE_WRITE_REG(hw, IXGBE_EEC, *eec);
	IXGBE_WRITE_FLUSH(hw);
	udelay(1);
}

static void ixmap_lower_eeprom_clk(struct ixmap_hw *hw, uint32_t *eec)
{
	/*
	 * Lower the clock input to the EEPROM (clearing the SK bit), then
	 * delay
	 */
	*eec = *eec & ~IXGBE_EEC_SK;
	IXGBE_WRITE_REG(hw, IXGBE_EEC, *eec);
	IXGBE_WRITE_FLUSH(hw);
	udelay(1);
}

static int32_t ixmap_get_eeprom_semaphore(struct ixmap_hw *hw)
{
	int32_t status = IXGBE_ERR_EEPROM;
	uint32_t timeout = 2000;
	uint32_t i;
	uint32_t swsm;

	/* Get SMBI software semaphore between device drivers first */
	for (i = 0; i < timeout; i++) {
		/*
		 * If the SMBI bit is 0 when we read it, then the bit will be
		 * set and we have the semaphore
		 */
		swsm = IXGBE_READ_REG(hw, IXGBE_SWSM);
		if (!(swsm & IXGBE_SWSM_SMBI)) {
			status = 0;
			break;
		}
		udelay(50);
	}

	if (i == timeout) {
		/*
		 * this release is particularly important because our attempts
		 * above to get the semaphore may have succeeded, and if there
		 * was a timeout, we should unconditionally clear the semaphore
		 * bits to free the driver to make progress
		 */
		ixmap_release_eeprom_semaphore(hw);

		udelay(50);
		/*
		 * one last try
		 * If the SMBI bit is 0 when we read it, then the bit will be
		 * set and we have the semaphore
		 */
		swsm = IXGBE_READ_REG(hw, IXGBE_SWSM);
		if (!(swsm & IXGBE_SWSM_SMBI))
			status = 0;
	}

	/* Now get the semaphore between SW/FW through the SWESMBI bit */
	if (status == 0) {
		for (i = 0; i < timeout; i++) {
			swsm = IXGBE_READ_REG(hw, IXGBE_SWSM);

			/* Set the SW EEPROM semaphore bit to request access */
			swsm |= IXGBE_SWSM_SWESMBI;
			IXGBE_WRITE_REG(hw, IXGBE_SWSM, swsm);

			/*
			 * If we set the bit successfully then we got the
			 * semaphore.
			 */
			swsm = IXGBE_READ_REG(hw, IXGBE_SWSM);
			if (swsm & IXGBE_SWSM_SWESMBI)
				break;

			udelay(50);
		}

		/*
		 * Release semaphores and return error if SW EEPROM semaphore
		 * was not granted because we don't have access to the EEPROM
		 */
		if (i >= timeout) {
			ixmap_release_eeprom_semaphore(hw);
			status = IXGBE_ERR_EEPROM;
		}
	}

	return status;
}

static void ixmap_release_eeprom_semaphore(struct ixmap_hw *hw)
{
	uint32_t swsm;

	swsm = IXGBE_READ_REG(hw, IXGBE_SWSM);

	/* Release both semaphores by writing 0 to the bits SWESMBI and SMBI */
	swsm &= ~(IXGBE_SWSM_SWESMBI | IXGBE_SWSM_SMBI);
	IXGBE_WRITE_REG(hw, IXGBE_SWSM, swsm);
	IXGBE_WRITE_FLUSH(hw);
}

int32_t ixmap_init_eeprom_params(struct ixmap_hw *hw)
{
	struct ixmap_eeprom_info *eeprom = &hw->eeprom;
	uint32_t eec;
	uint16_t eeprom_size;

	if (eeprom->type == ixmap_eeprom_uninitialized) {
		eeprom->type = ixmap_eeprom_none;
		/* Set default semaphore delay to 10ms which is a well
		 * tested value */
		eeprom->semaphore_delay = 10;
		/* Clear EEPROM page size, it will be initialized as needed */
		eeprom->word_page_size = 0;

		/*
		 * Check for EEPROM present first.
		 * If not present leave as none
		 */
		eec = IXGBE_READ_REG(hw, IXGBE_EEC);
		if (eec & IXGBE_EEC_PRES) {
			eeprom->type = ixmap_eeprom_spi;

			/*
			 * SPI EEPROM is assumed here.  This code would need to
			 * change if a future EEPROM is not SPI.
			 */
			eeprom_size = (uint16_t)((eec & IXGBE_EEC_SIZE) >>
					    IXGBE_EEC_SIZE_SHIFT);
			eeprom->word_size = 1 << (eeprom_size +
					     IXGBE_EEPROM_WORD_SIZE_SHIFT);
		}

		if (eec & IXGBE_EEC_ADDR_SIZE)
			eeprom->address_bits = 16;
		else
			eeprom->address_bits = 8;
	}

	return 0;
}

uint16_t ixmap_calc_eeprom_checksum(struct ixmap_hw *hw)
{
	uint16_t i;
	uint16_t j;
	uint16_t checksum = 0;
	uint16_t length = 0;
	uint16_t pointer = 0;
	uint16_t word = 0;

	/* Include 0x0-0x3F in the checksum */
	for (i = 0; i < IXGBE_EEPROM_CHECKSUM; i++) {
		if (hw->eeprom.ops.read(hw, i, &word) != 0) {
			break;
		}
		checksum += word;
	}

	/* Include all data from pointers except for the fw pointer */
	for (i = IXGBE_PCIE_ANALOG_PTR; i < IXGBE_FW_PTR; i++) {
		hw->eeprom.ops.read(hw, i, &pointer);

		/* Make sure the pointer seems valid */
		if (pointer != 0xFFFF && pointer != 0) {
			hw->eeprom.ops.read(hw, pointer, &length);

			if (length != 0xFFFF && length != 0) {
				for (j = pointer+1; j <= pointer+length; j++) {
					hw->eeprom.ops.read(hw, j, &word);
					checksum += word;
				}
			}
		}
	}

	checksum = (uint16_t)IXGBE_EEPROM_SUM - checksum;

	return checksum;
}
