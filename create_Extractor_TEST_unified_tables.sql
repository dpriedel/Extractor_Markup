-- this will create tables and related structures for
-- the SEC database which is extracted from the files
-- downloaded from the SEC FTP server.

-- see link below for hints
-- https://wiki.postgresql.org/wiki/Don%27t_Do_This

CREATE SCHEMA IF NOT EXISTS test_unified_extracts AUTHORIZATION extractor_pg;

DROP TABLE IF EXISTS test_unified_extracts.sec_filing_id CASCADE;

CREATE TABLE test_unified_extracts.sec_filing_id
(
    filing_id BIGINT GENERATED ALWAYS AS IDENTITY UNIQUE,
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
    period_context_id TEXT DEFAULT NULL,
    shares_outstanding NUMERIC DEFAULT -1,
    data_source TEXT NOT NULL,
    UNIQUE (cik, form_type, period_ending),
    PRIMARY KEY (cik, form_type, period_ending)
);

ALTER TABLE test_unified_extracts.sec_filing_id OWNER TO extractor_pg;

DROP INDEX IF EXISTS idx_period_ending;
CREATE INDEX idx_period_ending ON test_unified_extracts.sec_filing_id (period_ending);

DROP TABLE IF EXISTS test_unified_extracts.sec_bal_sheet_data;
DROP TABLE IF EXISTS test_unified_extracts.sec_stmt_of_ops_data;
DROP TABLE IF EXISTS test_unified_extracts.sec_cash_flows_data;
DROP TABLE IF EXISTS test_unified_extracts.sec_xbrl_data;

CREATE TABLE test_unified_extracts.sec_bal_sheet_data
(
    filing_data_id BIGINT GENERATED ALWAYS AS IDENTITY UNIQUE,
    filing_id BIGINT REFERENCES test_unified_extracts.sec_filing_id (filing_id) ON DELETE CASCADE,
    label TEXT NOT NULL,
    value NUMERIC(20, 4) NOT NULL,
    tsv_index_col TSVECTOR GENERATED ALWAYS AS (to_tsvector('pg_catalog.english', label)) STORED,
    PRIMARY KEY (filing_data_id)
);

ALTER TABLE test_unified_extracts.sec_bal_sheet_data OWNER TO extractor_pg;

DROP INDEX IF EXISTS idx_bal_sheet_val;
CREATE INDEX idx_bal_sheet_val ON test_unified_extracts.sec_bal_sheet_data USING gin (tsv_index_col);

DROP INDEX IF EXISTS idx_bal_sheet;
CREATE INDEX idx_bal_sheet ON test_unified_extracts.sec_bal_sheet_data (filing_id);

CREATE TABLE test_unified_extracts.sec_stmt_of_ops_data
(
    filing_data_id BIGINT GENERATED ALWAYS AS IDENTITY UNIQUE,
    filing_id BIGINT REFERENCES test_unified_extracts.sec_filing_id (filing_id) ON DELETE CASCADE,
    label TEXT NOT NULL,
    value NUMERIC(20, 4) NOT NULL,
    tsv_index_col TSVECTOR GENERATED ALWAYS AS (to_tsvector('pg_catalog.english', label)) STORED,
    PRIMARY KEY (filing_data_id)
);

ALTER TABLE test_unified_extracts.sec_stmt_of_ops_data OWNER TO extractor_pg;

DROP INDEX IF EXISTS idx_stmt_of_ops_val;
CREATE INDEX idx_stmt_of_ops_val ON test_unified_extracts.sec_stmt_of_ops_data USING gin (tsv_index_col);

DROP INDEX IF EXISTS idx_stmt_of_ops;
CREATE INDEX idx_stmt_of_ops ON test_unified_extracts.sec_stmt_of_ops_data (filing_id);

CREATE TABLE test_unified_extracts.sec_cash_flows_data
(
    filing_data_id BIGINT GENERATED ALWAYS AS IDENTITY UNIQUE,
    filing_id BIGINT REFERENCES test_unified_extracts.sec_filing_id (filing_id) ON DELETE CASCADE,
    label TEXT NOT NULL,
    value NUMERIC(20, 4) NOT NULL,
    tsv_index_col TSVECTOR GENERATED ALWAYS AS (to_tsvector('pg_catalog.english', label)) STORED,
    PRIMARY KEY (filing_data_id)
);

ALTER TABLE test_unified_extracts.sec_cash_flows_data OWNER TO extractor_pg;

DROP INDEX IF EXISTS idx_cash_flows_val;
CREATE INDEX idx_cash_flows_val ON test_unified_extracts.sec_cash_flows_data USING gin (tsv_index_col);

DROP INDEX IF EXISTS idx_cash_flows;
CREATE INDEX idx_cash_flows ON test_unified_extracts.sec_cash_flows_data (filing_id);

CREATE TABLE test_unified_extracts.sec_xbrl_data
(
    filing_data_id BIGINT GENERATED ALWAYS AS IDENTITY UNIQUE,
    filing_id BIGINT REFERENCES test_unified_extracts.sec_filing_id (filing_id) ON DELETE CASCADE,
    xbrl_label TEXT NOT NULL,
    label TEXT NOT NULL,
    value NUMERIC(20, 4) NOT NULL,
    context_id TEXT NOT NULL,
    period_begin DATE NOT NULL,
    period_end DATE NOT NULL,
    units TEXT NOT NULL,
    decimals TEXT,
    tsv_index_col TSVECTOR GENERATED ALWAYS AS (to_tsvector('pg_catalog.english', label)) STORED,
    PRIMARY KEY (filing_data_id)
);

ALTER TABLE test_unified_extracts.sec_xbrl_data OWNER TO extractor_pg;

DROP INDEX IF EXISTS idx_xbrl_data_val;
CREATE INDEX idx_xbrl_data_val ON test_unified_extracts.sec_xbrl_data USING gin (tsv_index_col);

DROP INDEX IF EXISTS idx_xbrl_data;
CREATE INDEX idx_xbrl_data ON test_unified_extracts.sec_xbrl_data (filing_id);

DROP INDEX IF EXISTS idx_xbrl_period_end;
CREATE INDEX idx_xbrl_period_end ON test_unified_extracts.sec_xbrl_data (period_end);
