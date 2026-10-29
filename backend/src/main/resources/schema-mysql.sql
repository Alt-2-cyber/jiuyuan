CREATE TABLE IF NOT EXISTS site (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  site_code VARCHAR(64) NOT NULL UNIQUE,
  name VARCHAR(128) NOT NULL,
  address VARCHAR(255),
  map_config JSON,
  status VARCHAR(32) NOT NULL,
  created_at DATETIME NOT NULL,
  updated_at DATETIME NOT NULL
);

CREATE TABLE IF NOT EXISTS person (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  person_code VARCHAR(64) NOT NULL UNIQUE,
  name VARCHAR(64),
  role VARCHAR(32) NOT NULL,
  gender VARCHAR(16),
  age INT,
  phone VARCHAR(32),
  medical_note VARCHAR(255),
  created_at DATETIME NOT NULL,
  updated_at DATETIME NOT NULL
);

CREATE TABLE IF NOT EXISTS device (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  device_code VARCHAR(64) NOT NULL UNIQUE,
  site_id BIGINT,
  person_id BIGINT,
  device_type VARCHAR(32) NOT NULL,
  firmware_version VARCHAR(64),
  battery_level INT,
  online_status VARCHAR(32) NOT NULL,
  last_seen_at DATETIME,
  created_at DATETIME NOT NULL,
  updated_at DATETIME NOT NULL,
  INDEX idx_device_site (site_id),
  INDEX idx_device_person (person_id)
);

CREATE TABLE IF NOT EXISTS vital_sample (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  site_id BIGINT NOT NULL,
  person_id BIGINT NOT NULL,
  device_id BIGINT NOT NULL,
  spo2 DECIMAL(5,2),
  heart_rate INT,
  systolic INT,
  diastolic INT,
  temperature DECIMAL(5,2),
  motion_state VARCHAR(32),
  signal_quality DECIMAL(5,4),
  battery_level INT,
  sampled_at DATETIME NOT NULL,
  created_at DATETIME NOT NULL,
  INDEX idx_vital_person_time (person_id, sampled_at),
  INDEX idx_vital_site_time (site_id, sampled_at)
);

CREATE TABLE IF NOT EXISTS env_sample (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  site_id BIGINT NOT NULL,
  device_id BIGINT NOT NULL,
  temperature DECIMAL(6,2),
  humidity DECIMAL(6,2),
  combustible_gas DECIMAL(8,2),
  combustible_unit VARCHAR(32),
  co DECIMAL(8,2),
  o2 DECIMAL(8,2),
  h2s DECIMAL(8,2),
  risk_level VARCHAR(32),
  sampled_at DATETIME NOT NULL,
  created_at DATETIME NOT NULL,
  INDEX idx_env_device_time (device_id, sampled_at),
  INDEX idx_env_site_time (site_id, sampled_at)
);

CREATE TABLE IF NOT EXISTS rescue_task (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  task_code VARCHAR(64) NOT NULL UNIQUE,
  site_id BIGINT NOT NULL,
  victim_person_id BIGINT NOT NULL,
  rescuer_person_id BIGINT,
  target_device_id BIGINT,
  status VARCHAR(32) NOT NULL,
  priority_level VARCHAR(32) NOT NULL,
  priority_score DECIMAL(8,4),
  reason VARCHAR(512),
  assigned_at DATETIME,
  started_at DATETIME,
  completed_at DATETIME,
  created_at DATETIME NOT NULL,
  updated_at DATETIME NOT NULL,
  INDEX idx_task_site_status (site_id, status),
  INDEX idx_task_priority (priority_level, priority_score)
);

CREATE TABLE IF NOT EXISTS cloud_victim_sample (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  device_id VARCHAR(128) NOT NULL,
  service_id VARCHAR(128),
  event_time DATETIME NOT NULL,
  seq INT,
  spo2 INT,
  heart_rate INT,
  systolic INT,
  battery INT,
  sos BOOLEAN,
  source VARCHAR(32) NOT NULL,
  raw_json TEXT,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_cloud_device_time (device_id, event_time),
  INDEX idx_cloud_created_at (created_at)
);
