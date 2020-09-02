
DELETE from unified_extracts.filing_data_total_revenue ;

INSERT INTO unified_extracts.filing_data_total_revenue (SELECT t1.filing_id, t1.cik, t1.form_type, t1.period_ending,  MAX(abs(t2.value::REAL)) AS total_revenue FROM unified_extracts.sec_filing_id AS t1 INNER JOIN unified_extracts.sec_stmt_of_ops_data AS t2
 ON t1.filing_id = t2.filing_id  WHERE t2.tsv_index_col @@ TO_TSQUERY('total <-> revenue') GROUP BY t1.cik, t1.period_ending, t1.form_type,  t1.filing_id ORDER BY t1.cik, t1.period_ending ASC );

SELECT * FROM unified_extracts.filing_data_total_revenue LIMIT 20 ;

DELETE from unified_extracts.filing_data_net_income ;

INSERT INTO unified_extracts.filing_data_net_income (SELECT t1.filing_id, t1.cik, t1.form_type, t1.period_ending,  MAX(abs(t2.value::REAL)) AS total_revenue FROM unified_extracts.sec_filing_id AS t1 INNER JOIN unified_extracts.sec_stmt_of_ops_data AS t2
 ON t1.filing_id = t2.filing_id  WHERE t2.tsv_index_col @@ TO_TSQUERY('net <-> income') GROUP BY t1.cik, t1.period_ending, t1.form_type,  t1.filing_id ORDER BY t1.cik, t1.period_ending ASC );

SELECT * FROM unified_extracts.filing_data_net_income LIMIT 20 ;

DELETE from unified_extracts.filing_data_interest_expense ;

INSERT INTO unified_extracts.filing_data_interest_expense (SELECT t1.filing_id, t1.cik, t1.form_type, t1.period_ending,  MAX(abs(t2.value::REAL)) AS total_revenue FROM unified_extracts.sec_filing_id AS t1 INNER JOIN unified_extracts.sec_stmt_of_ops_data AS t2
 ON t1.filing_id = t2.filing_id  WHERE t2.tsv_index_col @@ TO_TSQUERY('interest <-> expense') GROUP BY t1.cik, t1.period_ending, t1.form_type,  t1.filing_id ORDER BY t1.cik, t1.period_ending ASC );

SELECT * FROM unified_extracts.filing_data_interest_expense LIMIT 20 ;

DELETE from unified_extracts.filing_data_operating_expenses ;

INSERT INTO unified_extracts.filing_data_operating_expenses (SELECT t1.filing_id, t1.cik, t1.form_type, t1.period_ending,  MAX(abs(t2.value::REAL)) AS total_revenue FROM unified_extracts.sec_filing_id AS t1 INNER JOIN unified_extracts.sec_stmt_of_ops_data AS t2
 ON t1.filing_id = t2.filing_id  WHERE t2.tsv_index_col @@ TO_TSQUERY('operating <-> expense') GROUP BY t1.cik, t1.period_ending, t1.form_type,  t1.filing_id ORDER BY t1.cik, t1.period_ending ASC );

SELECT * FROM unified_extracts.filing_data_operating_expenses LIMIT 20 ;

DELETE from unified_extracts.filing_data_total_assets ;

INSERT INTO unified_extracts.filing_data_total_assets (SELECT t1.filing_id, t1.cik, t1.form_type, t1.period_ending,  MAX(abs(t2.value::REAL)) AS total_revenue FROM unified_extracts.sec_filing_id AS t1 INNER JOIN unified_extracts.sec_bal_sheet_data AS t2
 ON t1.filing_id = t2.filing_id  WHERE t2.tsv_index_col @@ TO_TSQUERY('total <-> assets') GROUP BY t1.cik, t1.period_ending, t1.form_type,  t1.filing_id ORDER BY t1.cik, t1.period_ending ASC );

SELECT * FROM unified_extracts.filing_data_total_assets LIMIT 20 ;

DELETE from unified_extracts.filing_data_total_liabilities ;

INSERT INTO unified_extracts.filing_data_total_liabilities (SELECT t1.filing_id, t1.cik, t1.form_type, t1.period_ending,  MAX(abs(t2.value::REAL)) AS total_revenue FROM unified_extracts.sec_filing_id AS t1 INNER JOIN unified_extracts.sec_bal_sheet_data AS t2
 ON t1.filing_id = t2.filing_id  WHERE t2.tsv_index_col @@ TO_TSQUERY('total <-> liabilities') GROUP BY t1.cik, t1.period_ending, t1.form_type,  t1.filing_id ORDER BY t1.cik, t1.period_ending ASC );

SELECT * FROM unified_extracts.filing_data_total_liabilities LIMIT 20 ;

