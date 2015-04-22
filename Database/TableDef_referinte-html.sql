
-- ####################################################
-- table referinte_html: contine listele de linkuri gasite in pagini_html
--

CREATE TABLE referinte_html (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  id_pagini_html INT UNSIGNED NOT NULL,
  link VARCHAR(255) NOT NULL,
  PRIMARY KEY (id),
  INDEX (id_pagini_html),
  FOREIGN KEY (id_pagini_html)
    REFERENCES pagini_html(id)
) ENGINE=XtraDB;
