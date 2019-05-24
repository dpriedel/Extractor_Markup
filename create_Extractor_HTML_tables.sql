-- this will create tables and related structures for
-- the SEC database which is extracted from the files
-- downloaded from the SEC FTP server.

-- see link below for hints
-- https://wiki.postgresql.org/wiki/Don%27t_Do_This

DROP TABLE IF EXISTS html_extracts.sec_filing_id CASCADE ;

CREATE TABLE html_extracts.sec_filing_id
(
    filing_ID bigint GENERATED ALWAYS AS IDENTITY UNIQUE,
	cik TEXT NOT NULL,
	company_name TEXT NOT NULL,
	file_name TEXT NOT NULL,
	symbol TEXT ,
    sic TEXT NOT NULL,
	form_type TEXT NOT NULL,
	date_filed DATE NOT NULL,
	period_ending DATE NOT NULL,
    shares_outstanding NUMERIC DEFAULT 0,
	UNIQUE(cik, form_type, period_ending),
    PRIMARY KEY(cik, form_type, period_ending)
);

ALTER TABLE html_extracts.sec_filing_id OWNER TO extractor_pg;


DROP TABLE IF EXISTS html_extracts.sec_bal_sheet_data ;
DROP TABLE IF EXISTS html_extracts.sec_stmt_of_ops_data ;
DROP TABLE IF EXISTS html_extracts.sec_cash_flows_data ;

-- this definition is just a dummy placeholder for now.
-- the main thing was to get the foreign key stuff in plance.

CREATE TABLE html_extracts.sec_bal_sheet_data
(
    filing_data_ID bigint GENERATED ALWAYS AS IDENTITY UNIQUE,
	filing_ID bigint REFERENCES html_extracts.sec_filing_id (filing_ID) ON DELETE CASCADE,
	html_label TEXT NOT NULL,
    html_value TEXT NOT NULL,
	/* period_begin DATE NOT NULL, */
	/* period_end DATE NOT NULL, */
	/* units TEXT NOT NULL, */
	/* decimals TEXT, */
	tsv TSVECTOR,
	PRIMARY KEY(filing_data_ID)
);

ALTER TABLE html_extracts.sec_bal_sheet_data OWNER TO extractor_pg;

DROP TRIGGER IF EXISTS tsv_update ON html_extracts.sec_bal_sheet_data ;

CREATE TRIGGER tsv_update
	BEFORE INSERT OR UPDATE ON html_extracts.sec_bal_sheet_data FOR EACH ROW
	EXECUTE PROCEDURE
		tsvector_update_trigger(tsv, 'pg_catalog.english', html_label);

DROP INDEX IF EXISTS idx_bal_sheet ;

CREATE INDEX idx_bal_sheet ON html_extracts.sec_bal_sheet_data USING GIN (tsv);

CREATE TABLE html_extracts.sec_stmt_of_ops_data
(
    filing_data_ID bigint GENERATED ALWAYS AS IDENTITY UNIQUE,
	filing_ID bigint REFERENCES html_extracts.sec_filing_id (filing_ID) ON DELETE CASCADE,
	html_label TEXT NOT NULL,
    html_value TEXT NOT NULL,
	/* period_begin DATE NOT NULL, */
	/* period_end DATE NOT NULL, */
	/* units TEXT NOT NULL, */
	/* decimals TEXT, */
	tsv TSVECTOR,
	PRIMARY KEY(filing_data_ID)
);

ALTER TABLE html_extracts.sec_stmt_of_ops_data OWNER TO extractor_pg;

DROP TRIGGER IF EXISTS tsv_update ON html_extracts.sec_stmt_of_ops_data ;

CREATE TRIGGER tsv_update
	BEFORE INSERT OR UPDATE ON html_extracts.sec_stmt_of_ops_data FOR EACH ROW
	EXECUTE PROCEDURE
		tsvector_update_trigger(tsv, 'pg_catalog.english', html_label);

DROP INDEX IF EXISTS idx_stmt_of_ops ;

CREATE INDEX idx_stmt_of_ops ON html_extracts.sec_stmt_of_ops_data USING GIN (tsv);

CREATE TABLE html_extracts.sec_cash_flows_data
(
    filing_data_ID bigint GENERATED ALWAYS AS IDENTITY UNIQUE,
	filing_ID bigint REFERENCES html_extracts.sec_filing_id (filing_ID) ON DELETE CASCADE,
	html_label TEXT NOT NULL,
    html_value TEXT NOT NULL,
	/* period_begin DATE NOT NULL, */
	/* period_end DATE NOT NULL, */
	/* units TEXT NOT NULL, */
	/* decimals TEXT, */
	tsv TSVECTOR,
	PRIMARY KEY(filing_data_ID)
);

ALTER TABLE html_extracts.sec_cash_flows_data OWNER TO extractor_pg;

DROP TRIGGER IF EXISTS tsv_update ON html_extracts.sec_cash_flows_data ;

CREATE TRIGGER tsv_update
	BEFORE INSERT OR UPDATE ON html_extracts.sec_cash_flows_data FOR EACH ROW
	EXECUTE PROCEDURE
		tsvector_update_trigger(tsv, 'pg_catalog.english', html_label);

DROP INDEX IF EXISTS idx_cash_flows ;

CREATE INDEX idx_cash_flows ON html_extracts.sec_cash_flows_data USING GIN (tsv);

