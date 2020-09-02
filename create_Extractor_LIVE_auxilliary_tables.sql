-- this will create tables and related structures for
-- the SEC database which is extracted from the files
-- downloaded from the SEC FTP server.

-- see link below for hints
-- https://wiki.postgresql.org/wiki/Don%27t_Do_This

DROP TABLE IF EXISTS live_unified_extracts.filing_data CASCADE ;

CREATE TABLE live_unified_extracts.filing_data
(
	filing_ID bigint REFERENCES live_unified_extracts.sec_filing_id (filing_ID) ON DELETE CASCADE,
	cik TEXT NOT NULL,
	symbol TEXT DEFAULT NULL,
	form_type TEXT NOT NULL,
	period_ending DATE NOT NULL,
	period_context_ID TEXT DEFAULT NULL,
    total_assets NUMERIC(20,4) DEFAULT NULL,
    total_liabilities NUMERIC(20,4) DEFAULT NULL,
    total_revenue NUMERIC(20,4) DEFAULT NULL,
    operating_expenses NUMERIC(20,4) DEFAULT NULL,
    net_income NUMERIC(20,4) DEFAULT NULL,
    interest_expense NUMERIC(20,4) DEFAULT NULL,
	UNIQUE(cik, form_type, period_ending),
    PRIMARY KEY(cik, form_type, period_ending)
);

ALTER TABLE live_unified_extracts.filing_data OWNER TO extractor_pg;

DROP TABLE IF EXISTS live_unified_extracts.filing_data_total_assets CASCADE ;

CREATE TABLE live_unified_extracts.filing_data_total_assets
(
	filing_ID bigint REFERENCES live_unified_extracts.sec_filing_id (filing_ID) ON DELETE CASCADE,
	cik TEXT NOT NULL,
	form_type TEXT NOT NULL,
	period_ending DATE NOT NULL,
    total_assets NUMERIC(20,4) DEFAULT NULL,
	UNIQUE(cik, form_type, period_ending),
    PRIMARY KEY(cik, form_type, period_ending)
);

ALTER TABLE live_unified_extracts.filing_data_total_assets OWNER TO extractor_pg;

DROP TABLE IF EXISTS live_unified_extracts.filing_data_total_liabilities CASCADE ;

CREATE TABLE live_unified_extracts.filing_data_total_liabilities
(
	filing_ID bigint REFERENCES live_unified_extracts.sec_filing_id (filing_ID) ON DELETE CASCADE,
	cik TEXT NOT NULL,
	form_type TEXT NOT NULL,
	period_ending DATE NOT NULL,
    total_liabilities NUMERIC(20,4) DEFAULT NULL,
	UNIQUE(cik, form_type, period_ending),
    PRIMARY KEY(cik, form_type, period_ending)
);

ALTER TABLE live_unified_extracts.filing_data_total_liabilities OWNER TO extractor_pg;

DROP TABLE IF EXISTS live_unified_extracts.filing_data_total_revenue CASCADE ;

CREATE TABLE live_unified_extracts.filing_data_total_revenue
(
	filing_ID bigint REFERENCES live_unified_extracts.sec_filing_id (filing_ID) ON DELETE CASCADE,
	cik TEXT NOT NULL,
	form_type TEXT NOT NULL,
	period_ending DATE NOT NULL,
    total_revenue NUMERIC(20,4) DEFAULT NULL,
	UNIQUE(cik, form_type, period_ending),
    PRIMARY KEY(cik, form_type, period_ending)
);

ALTER TABLE live_unified_extracts.filing_data_total_revenue OWNER TO extractor_pg;

DROP TABLE IF EXISTS live_unified_extracts.filing_data_operating_expenses CASCADE ;

CREATE TABLE live_unified_extracts.filing_data_operating_expenses
(
	filing_ID bigint REFERENCES live_unified_extracts.sec_filing_id (filing_ID) ON DELETE CASCADE,
	cik TEXT NOT NULL,
	form_type TEXT NOT NULL,
	period_ending DATE NOT NULL,
    operating_expenses NUMERIC(20,4) DEFAULT NULL,
	UNIQUE(cik, form_type, period_ending),
    PRIMARY KEY(cik, form_type, period_ending)
);

ALTER TABLE live_unified_extracts.filing_data_operating_expenses OWNER TO extractor_pg;

DROP TABLE IF EXISTS live_unified_extracts.filing_data_net_income CASCADE ;

CREATE TABLE live_unified_extracts.filing_data_net_income
(
	filing_ID bigint REFERENCES live_unified_extracts.sec_filing_id (filing_ID) ON DELETE CASCADE,
	cik TEXT NOT NULL,
	form_type TEXT NOT NULL,
	period_ending DATE NOT NULL,
    net_income NUMERIC(20,4) DEFAULT NULL,
	UNIQUE(cik, form_type, period_ending),
    PRIMARY KEY(cik, form_type, period_ending)
);

ALTER TABLE live_unified_extracts.filing_data_net_income OWNER TO extractor_pg;

DROP TABLE IF EXISTS live_unified_extracts.filing_data_interest_expense CASCADE ;

CREATE TABLE live_unified_extracts.filing_data_interest_expense
(
	filing_ID bigint REFERENCES live_unified_extracts.sec_filing_id (filing_ID) ON DELETE CASCADE,
	cik TEXT NOT NULL,
	form_type TEXT NOT NULL,
	period_ending DATE NOT NULL,
    interest_expense NUMERIC(20,4) DEFAULT NULL,
	UNIQUE(cik, form_type, period_ending),
    PRIMARY KEY(cik, form_type, period_ending)
);

ALTER TABLE live_unified_extracts.filing_data_interest_expense OWNER TO extractor_pg;


