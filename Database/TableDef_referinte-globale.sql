-- ##############################################
-- table referinte_globale:
--

CREATE TABLE referinte_globale (
  ID BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  URL VARCHAR(500) NOT NULL,
  Description Varchar(500),
  Data TEXT,
  RefreshDate TIMESTAMP,
  PRIMARY KEY(ID),
) ENGINE=XtraDB;
