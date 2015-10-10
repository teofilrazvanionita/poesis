
-- ####################################################
-- table referinte_html: contine listele de linkuri gasite in pagini_html
--

CREATE TABLE referinte_html (
  ID BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  ID_pagini_html BIGINT UNSIGNED NOT NULL,
  Link VARCHAR(500) NOT NULL,
  PRIMARY KEY (ID),
  INDEX (ID_pagini_html),
  FOREIGN KEY (ID_pagini_html)
    REFERENCES pagini_html(ID)
) ENGINE=XtraDB;
