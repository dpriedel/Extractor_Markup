-- this will create tables and related structures for
-- the SEC database which is extracted from the files
-- downloaded from the SEC FTP server.

DROP TABLE IF EXISTS xbrl_extracts.sec_filing_id CASCADE;

CREATE TABLE xbrl_extracts.sec_filing_id
(
    filing_id BIGINT GENERATED ALWAYS AS IDENTITY UNIQUE,
    cik TEXT NOT NULL,
    alternate_id1 TEXT,
    alternate_id2 TEXT,
    company_name TEXT NOT NULL,
    file_name TEXT NOT NULL,
    symbol TEXT,
    sic TEXT NOT NULL,
    form_type TEXT NOT NULL,
    date_filed DATE NOT NULL,
    period_ending DATE NOT NULL,
    period_context_id TEXT NOT NULL,
    shares_outstanding NUMERIC DEFAULT 0,
    UNIQUE (cik, form_type, period_ending),
    PRIMARY KEY (cik, form_type, period_ending)
);

ALTER TABLE xbrl_extracts.sec_filing_id OWNER TO extractor_pg;


DROP TABLE IF EXISTS xbrl_extracts.sec_filing_data;

-- this definition is just a dummy placeholder for now.
-- the main thing was to get the foreign key stuff in plance.

CREATE TABLE xbrl_extracts.sec_filing_data
(
    filing_data_id BIGINT GENERATED ALWAYS AS IDENTITY UNIQUE,
    filing_id BIGINT REFERENCES xbrl_extracts.sec_filing_id (filing_id) ON DELETE CASCADE,
    xbrl_label TEXT NOT NULL,
    user_label TEXT NOT NULL,
    xbrl_value TEXT NOT NULL,
    context_id TEXT NOT NULL,
    period_begin DATE NOT NULL,
    period_end DATE NOT NULL,
    units TEXT NOT NULL,
    decimals TEXT,
    tsv TSVECTOR,
    PRIMARY KEY (filing_data_id)
);

ALTER TABLE xbrl_extracts.sec_filing_data OWNER TO extractor_pg;

DROP TRIGGER IF EXISTS tsv_update ON xbrl_extracts.sec_filing_data;

CREATE TRIGGER tsv_update
BEFORE INSERT OR UPDATE ON xbrl_extracts.sec_filing_data FOR EACH ROW
EXECUTE PROCEDURE
tsvector_update_trigger(tsv, 'pg_catalog.english', user_label);

DROP INDEX IF EXISTS idx_field_label;

CREATE INDEX idx_field_label ON xbrl_extracts.sec_filing_data USING gin (tsv);
