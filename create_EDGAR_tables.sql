-- this will create tables and related structures for
-- the EDGAR database which is extracted from the files
-- downloaded from the SEC EDGAR FTP server.

DROP TABLE IF EXISTS xbrl_extracts.edgar_filing_id CASCADE ;

CREATE TABLE xbrl_extracts.edgar_filing_id
(
	filing_ID SERIAL UNIQUE,
	cik TEXT NOT NULL,
	company_name TEXT NOT NULL,
	file_name TEXT NOT NULL,
	symbol TEXT ,
    sic TEXT NOT NULL,
	form_type TEXT NOT NULL,
	date_filed DATE NOT NULL,
	period_ending DATE NOT NULL,
    shares_outstanding BIGINT DEFAULT 0,
	UNIQUE(cik, form_type, period_ending),
    PRIMARY KEY(cik, form_type, period_ending)
);

ALTER TABLE xbrl_extracts.edgar_filing_id OWNER TO edgar_pg;


DROP TABLE IF EXISTS xbrl_extracts.edgar_filing_data ;

-- this definition is just a dummy placeholder for now.
-- the main thing was to get the foreign key stuff in plance.

CREATE TABLE xbrl_extracts.edgar_filing_data
(
	filing_data_ID SERIAL UNIQUE,
	filing_ID integer REFERENCES xbrl_extracts.edgar_filing_id (filing_ID) ON DELETE CASCADE,
	symbol TEXT ,
    sic TEXT NOT NULL,
	date_filed DATE NOT NULL,
    shares_outstanding BIGINT DEFAULT 0,
	PRIMARY KEY(filing_ID)
);

ALTER TABLE xbrl_extracts.edgar_filing_data OWNER TO edgar_pg;
