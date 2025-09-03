-- this will create and index table for the form files
-- downloaded from the SEC EDGAR FTP server.

-- see link below for hints
-- https://wiki.postgresql.org/wiki/Don%27t_Do_This

CREATE SCHEMA IF NOT EXISTS test_forms_index AUTHORIZATION extractor_pg;

DROP TABLE IF EXISTS test_forms_index.sec_form_index CASCADE;

CREATE TABLE test_forms_index.sec_form_index
(
    filing_id BIGINT GENERATED ALWAYS AS IDENTITY UNIQUE,
    cik TEXT NOT NULL,
    alternate_id1 TEXT DEFAULT NULL,
    alternate_id2 TEXT DEFAULT NULL,
    company_name TEXT NOT NULL,
    file_name TEXT DEFAULT NULL,
    symbol TEXT DEFAULT NULL,
    sic TEXT NOT NULL,
    form_type TEXT NOT NULL,
    date_filed DATE DEFAULT NULL,
    period_ending DATE NOT NULL,
    period_context_id TEXT DEFAULT NULL,
    has_html BOOL NOT NULL,
    has_xml BOOL NOT NULL,
    has_xls BOOL NOT NULL,
    UNIQUE (cik, form_type, period_ending),
    PRIMARY KEY (cik, form_type, period_ending)
);

ALTER TABLE test_forms_index.sec_form_index OWNER TO extractor_pg;

DROP INDEX IF EXISTS idx_period_ending;
CREATE INDEX idx_period_ending ON test_forms_index.sec_form_index (period_ending);
