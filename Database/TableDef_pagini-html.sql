-- ##############################################
-- table pagini_html:
--

CREATE TABLE pagini_html (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  IP VARCHAR(15) NOT NULL,
  titlu VARCHAR(255) NOT NULL,
  PRIMARY KEY(id),
  INDEX (id)
) ENGINE=XtraDB;
