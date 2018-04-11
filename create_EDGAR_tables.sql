-- this will create tables and related structures for
-- the EDGAR database which is extracted from the files
-- downloaded from the SEC EDGAR FTP server.

DROP TABLE IF EXISTS xbrl_extracts.edgar_company_id CASCADE ;

CREATE TABLE xbrl_extracts.edgar_company_id
(
	company_ID SERIAL PRIMARY KEY UNIQUE,
	cik TEXT NOT NULL,
	company_name TEXT NOT NULL,
	file_name TEXT UNIQUE NOT NULL
);

ALTER TABLE xbrl_extracts.edgar_company_id OWNER TO edgar_pg;


DROP TABLE IF EXISTS xbrl_extracts.edgar_filing_data ;

CREATE TABLE xbrl_extracts.edgar_filing_data
(
	filing_ID SERIAL PRIMARY KEY UNIQUE,
	company_id integer REFERENCES xbrl_extracts.edgar_company_id ON DELETE CASCADE,
	symbol TEXT NOT NULL,
    sic TEXT NOT NULL,
	form_type TEXT NOT NULL,
	date_filed DATE NOT NULL,
	quarter_ending DATE NOT NULL
);

ALTER TABLE xbrl_extracts.edgar_filing_data OWNER TO edgar_pg;
