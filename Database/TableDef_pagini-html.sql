-- ##############################################
-- table pagini_html:
--

CREATE TABLE pagini_html (
  ID INT UNSIGNED NOT NULL AUTO_INCREMENT,
  RefererIP VARCHAR(15) NOT NULL,
  RefererLink Varchar(500),
  Title VARCHAR(255),
  WR_THREAD TINYINT NOT NULL,
  PRIMARY KEY(RefererIP),
  INDEX (id)
) ENGINE=XtraDB;
