CREATE TABLE IF NOT EXISTS poeta_journey (
 player_id INT NOT NULL PRIMARY KEY,
 decision ENUM('PLAY','SKIP') NOT NULL,
 chosen_class VARCHAR(20) NULL,
 start_exp BIGINT NOT NULL DEFAULT 0,
 quest_expands INT NOT NULL DEFAULT 0,
 completed_quests TEXT NULL,
 welcome_pending BOOLEAN NOT NULL DEFAULT 0,
 journey_version INT NOT NULL DEFAULT 1,
 needs_recovery BOOLEAN NOT NULL DEFAULT 0,
 chosen_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
 CONSTRAINT poeta_journey_player FOREIGN KEY (player_id) REFERENCES players(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
