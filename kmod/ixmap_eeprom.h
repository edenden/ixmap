int32_t ixmap_acquire_swfw_sync(struct ixmap_hw *hw, uint16_t mask);
void ixmap_release_swfw_sync(struct ixmap_hw *hw, uint16_t mask);
int32_t ixmap_validate_eeprom_checksum(struct ixmap_hw *hw,
	uint16_t *checksum_val);
int32_t ixmap_read_eerd(struct ixmap_hw *hw,
	uint16_t offset, uint16_t *data);
int32_t ixmap_read_eerd_buffer(struct ixmap_hw *hw,
	uint16_t offset, uint16_t words, uint16_t *data);
int32_t ixmap_read_eeprom_bit_bang(struct ixmap_hw *hw,
	uint16_t offset, uint16_t *data);
int32_t ixmap_init_uta_tables(struct ixmap_hw *hw);
int32_t ixmap_poll_eerd_eewr_done(struct ixmap_hw *hw, uint32_t ee_reg);
int32_t ixmap_init_eeprom_params(struct ixmap_hw *hw);
uint16_t ixmap_calc_eeprom_checksum(struct ixmap_hw *hw);
int32_t ixmap_read_pba_string(struct ixmap_hw *hw,
	uint8_t *pba_num, uint32_t pba_num_size);

