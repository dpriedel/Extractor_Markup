-- this will create tables and related structures for
-- the EDGAR database which is extracted from the files
-- downloaded from the SEC EDGAR FTP server.

DROP TABLE IF EXISTS xbrl_extracts.edgar_company_id CASCADE ;

CREATE TABLE xbrl_extracts.edgar_company_id
(
	cik TEXT NOT NULL UNIQUE PRIMARY KEY,
	company_name TEXT NOT NULL,
	file_name TEXT NOT NULL
);

ALTER TABLE xbrl_extracts.edgar_company_id OWNER TO edgar_pg;


DROP TABLE IF EXISTS xbrl_extracts.edgar_filing_data ;

CREATE TABLE xbrl_extracts.edgar_filing_data
(
	filing_ID SERIAL,
	cik text REFERENCES xbrl_extracts.edgar_company_id ON DELETE CASCADE,
	symbol TEXT ,
    sic TEXT NOT NULL,
	form_type TEXT NOT NULL,
	date_filed DATE NOT NULL,
	period_ending DATE NOT NULL,
    shares_outstanding BIGINT DEFAULT 0,
    PRIMARY KEY(cik, filing_ID)
);

ALTER TABLE xbrl_extracts.edgar_filing_data OWNER TO edgar_pg;
