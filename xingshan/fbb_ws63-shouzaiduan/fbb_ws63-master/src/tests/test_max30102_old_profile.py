import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DRIVER_PATH = ROOT / "application/samples/products/sle_uart/max30102.c"
RELAY_PATH = ROOT / "application/samples/products/sle_relay/sle_relay.c"
RELAY_CMAKE_PATH = ROOT / "application/samples/products/sle_relay/CMakeLists.txt"


def read_source(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def macro_value(source: str, name: str) -> str:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+([^\s]+)", source, re.MULTILINE)
    if match is None:
        raise AssertionError(f"missing macro: {name}")
    return match.group(1).rstrip("UuLl")


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"missing function: {signature}")
    brace = source.find("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]
    raise AssertionError(f"unterminated function: {signature}")


class Max30102OldProfileContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.driver = read_source(DRIVER_PATH)
        cls.relay = read_source(RELAY_PATH)
        cls.relay_cmake = read_source(RELAY_CMAKE_PATH)

    def test_relay_uses_old_working_i2c_profile(self) -> None:
        self.assertEqual(macro_value(self.relay, "RESCUE_VICTIM_I2C_SDA_PIN"), "15")
        self.assertEqual(macro_value(self.relay, "RESCUE_VICTIM_I2C_SCL_PIN"), "16")
        self.assertEqual(macro_value(self.relay, "RESCUE_VICTIM_I2C_BUS"), "1")
        self.assertEqual(macro_value(self.relay, "RESCUE_VICTIM_I2C_BAUDRATE"), "400000")
        self.assertEqual(macro_value(self.relay, "RESCUE_VICTIM_SENSOR_POLL_MS"), "40")

        prepare = function_body(self.relay, "static int rescue_victim_i2c_prepare(void)")
        self.assertIn(
            "uapi_i2c_master_init(RESCUE_VICTIM_I2C_BUS, RESCUE_VICTIM_I2C_BAUDRATE,",
            prepare,
        )

    def test_driver_uses_old_register_and_signal_profile(self) -> None:
        expected = {
            "MAX30102_I2C_ADDR": "0x57",
            "MAX30102_EXPECTED_PART_ID": "0x15",
            "MAX30102_LED_CURRENT": "0x24",
            "MAX30102_SAMPLE_PERIOD_MS": "40",
            "MAX30102_FINGER_IR_MIN": "50000",
            "MAX30102_PEAK_MIN_MS": "320",
            "MAX30102_PEAK_MAX_MS": "1800",
        }
        for name, value in expected.items():
            with self.subTest(name=name):
                self.assertEqual(macro_value(self.driver, name), value)

    def test_init_keeps_old_retry_and_register_sequence(self) -> None:
        body = function_body(self.driver, "int max30102_init(i2c_bus_t bus)")
        expected_in_order = [
            "retry < 3",
            "max30102_read_reg_raw(MAX30102_REG_PART_ID, &part_id)",
            "osal_msleep(30)",
            "max30102_write_reg(MAX30102_REG_MODE_CONFIG, 0x40)",
            "osal_msleep(20)",
            "max30102_write_reg(MAX30102_REG_FIFO_WR_PTR, 0x00)",
            "max30102_write_reg(MAX30102_REG_OVF_COUNTER, 0x00)",
            "max30102_write_reg(MAX30102_REG_FIFO_RD_PTR, 0x00)",
            "max30102_write_reg(MAX30102_REG_FIFO_CONFIG, 0x4F)",
            "max30102_write_reg(MAX30102_REG_MODE_CONFIG, 0x03)",
            "max30102_write_reg(MAX30102_REG_SPO2_CONFIG, 0x27)",
            "max30102_write_reg(MAX30102_REG_LED1_PA, MAX30102_LED_CURRENT)",
            "max30102_write_reg(MAX30102_REG_LED2_PA, MAX30102_LED_CURRENT)",
        ]
        position = -1
        for expression in expected_in_order:
            next_position = body.find(expression, position + 1)
            self.assertGreater(next_position, position, f"missing or out of order: {expression}")
            position = next_position

    def test_processing_matches_old_real_sensor_algorithm(self) -> None:
        pulse = function_body(self.driver, "static void max30102_update_pulse(uint32_t red, uint32_t ir)")
        self.assertRegex(pulse, r"g_ir_dc\s*=\s*\(\(g_ir_dc\s*\*\s*31U?\)\s*\+\s*ir\)\s*/\s*32U?")
        self.assertRegex(pulse, r"threshold\s*=\s*\(int32_t\)g_ir_dc\s*/\s*120")
        self.assertIn("if (threshold < 600)", pulse)
        self.assertNotIn("max30102_update_demo_hr", self.driver)

        spo2 = function_body(self.driver, "static void max30102_update_spo2(uint32_t red, uint32_t ir)")
        self.assertIn("spo2 = 110 - (ratio / 2)", spo2)
        self.assertIn("max30102_clamp_int(spo2, 85, 99)", spo2)

    def test_relay_build_links_shared_driver(self) -> None:
        self.assertIn("${SLE_UART_SAMPLE_DIR}/max30102.c", self.relay_cmake)
        self.assertIn("max30102_init(RESCUE_VICTIM_I2C_BUS)", self.relay)
        self.assertIn("max30102_update()", self.relay)


if __name__ == "__main__":
    unittest.main()
