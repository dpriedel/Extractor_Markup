-- this will create tables and related structures for
-- the SEC database which is extracted from the files
-- downloaded from the SEC FTP server.

-- see link below for hints
-- https://wiki.postgresql.org/wiki/Don%27t_Do_This

CREATE SCHEMA IF NOT EXISTS unified_extracts AUTHORIZATION extractor_pg;

DROP TABLE IF EXISTS unified_extracts.sec_filing_id CASCADE ;

CREATE TABLE unified_extracts.sec_filing_id
(
    filing_ID bigint GENERATED ALWAYS AS IDENTITY UNIQUE,
	cik TEXT NOT NULL,
	alternate_id1 TEXT DEFAULT NULL,
	alternate_id2 TEXT DEFAULT NULL,
	company_name TEXT NOT NULL,
	file_name TEXT DEFAULT NULL,
	amended_file_name TEXT DEFAULT NULL,
	symbol TEXT DEFAULT NULL,
    sic TEXT NOT NULL,
	form_type TEXT NOT NULL,
	date_filed DATE DEFAULT NULL,
	amended_date_filed DATE DEFAULT NULL,
	period_ending DATE NOT NULL,
	period_context_ID TEXT DEFAULT NULL,
    shares_outstanding NUMERIC DEFAULT -1,
    data_source TEXT NOT NULL,
	UNIQUE(cik, form_type, period_ending),
    PRIMARY KEY(cik, form_type, period_ending)
);

ALTER TABLE unified_extracts.sec_filing_id OWNER TO extractor_pg;

DROP INDEX IF EXISTS idx_period_ending ;
CREATE INDEX idx_period_ending ON unified_extracts.sec_filing_id (period_ending);

DROP TABLE IF EXISTS unified_extracts.sec_bal_sheet_data ;
DROP TABLE IF EXISTS unified_extracts.sec_stmt_of_ops_data ;
DROP TABLE IF EXISTS unified_extracts.sec_cash_flows_data ;
DROP TABLE IF EXISTS unified_extracts.sec_xbrl_data ;

CREATE TABLE unified_extracts.sec_bal_sheet_data
(
    filing_data_ID bigint GENERATED ALWAYS AS IDENTITY UNIQUE,
	filing_ID bigint REFERENCES unified_extracts.sec_filing_id (filing_ID) ON DELETE CASCADE,
	label TEXT NOT NULL,
    value NUMERIC(20,4) NOT NULL,
    tsv_index_col tsvector GENERATED ALWAYS AS (to_tsvector('pg_catalog.english', label)) STORED,
	PRIMARY KEY(filing_data_ID)
);

ALTER TABLE unified_extracts.sec_bal_sheet_data OWNER TO extractor_pg;

DROP INDEX IF EXISTS idx_bal_sheet_val ;
CREATE INDEX idx_bal_sheet_val ON unified_extracts.sec_bal_sheet_data USING GIN (tsv_index_col);

DROP INDEX IF EXISTS idx_bal_sheet ;
CREATE INDEX idx_bal_sheet ON unified_extracts.sec_bal_sheet_data (filing_ID);

CREATE TABLE unified_extracts.sec_stmt_of_ops_data
(
    filing_data_ID bigint GENERATED ALWAYS AS IDENTITY UNIQUE,
	filing_ID bigint REFERENCES unified_extracts.sec_filing_id (filing_ID) ON DELETE CASCADE,
	label TEXT NOT NULL,
    value NUMERIC(20,4) NOT NULL,
    tsv_index_col tsvector GENERATED ALWAYS AS (to_tsvector('pg_catalog.english', label)) STORED,
	PRIMARY KEY(filing_data_ID)
);

ALTER TABLE unified_extracts.sec_stmt_of_ops_data OWNER TO extractor_pg;

DROP INDEX IF EXISTS idx_stmt_of_ops_val ;
CREATE INDEX idx_stmt_of_ops_val ON unified_extracts.sec_stmt_of_ops_data USING GIN (tsv_index_col);

DROP INDEX IF EXISTS idx_stmt_of_ops ;
CREATE INDEX idx_stmt_of_ops ON unified_extracts.sec_stmt_of_ops_data (filing_ID);

CREATE TABLE unified_extracts.sec_cash_flows_data
(
    filing_data_ID bigint GENERATED ALWAYS AS IDENTITY UNIQUE,
	filing_ID bigint REFERENCES unified_extracts.sec_filing_id (filing_ID) ON DELETE CASCADE,
	label TEXT NOT NULL,
    value NUMERIC(20,4) NOT NULL,
    tsv_index_col tsvector GENERATED ALWAYS AS (to_tsvector('pg_catalog.english', label)) STORED,
	PRIMARY KEY(filing_data_ID)
);

ALTER TABLE unified_extracts.sec_cash_flows_data OWNER TO extractor_pg;

DROP INDEX IF EXISTS idx_cash_flows_val ;
CREATE INDEX idx_cash_flows_val ON unified_extracts.sec_cash_flows_data USING GIN (tsv_index_col);

DROP INDEX IF EXISTS idx_cash_flows ;
CREATE INDEX idx_cash_flows ON unified_extracts.sec_cash_flows_data (filing_ID);

CREATE TABLE unified_extracts.sec_xbrl_data
(
    filing_data_ID bigint GENERATED ALWAYS AS IDENTITY UNIQUE,
	filing_ID bigint REFERENCES unified_extracts.sec_filing_id (filing_ID) ON DELETE CASCADE,
	xbrl_label TEXT NOT NULL,
	label TEXT NOT NULL,
    value NUMERIC(20,4) NOT NULL,
	context_ID TEXT NOT NULL,
	period_begin DATE NOT NULL,
	period_end DATE NOT NULL,
	units TEXT NOT NULL,
	decimals TEXT,
    tsv_index_col tsvector GENERATED ALWAYS AS (to_tsvector('pg_catalog.english', label)) STORED,
	PRIMARY KEY(filing_data_ID)
);

ALTER TABLE unified_extracts.sec_xbrl_data OWNER TO extractor_pg;

DROP INDEX IF EXISTS idx_xbrl_data_val ;
CREATE INDEX idx_xbrl_data_val ON unified_extracts.sec_xbrl_data USING GIN (tsv_index_col);

DROP INDEX IF EXISTS idx_xbrl_data ;
CREATE INDEX idx_xbrl_data ON unified_extracts.sec_xbrl_data (filing_ID);

DROP INDEX IF EXISTS idx_xbrl_period_end ;
CREATE INDEX idx_xbrl_period_end ON unified_extracts.sec_xbrl_data (period_end);

