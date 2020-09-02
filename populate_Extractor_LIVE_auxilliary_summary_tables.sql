DELETE FROM live_unified_extracts.filing_data ;

INSERT INTO live_unified_extracts.filing_data (filing_ID, cik, symbol, form_type, period_ending, period_context_ID, total_assets) (SELECT t1.filing_ID, t3.cik, t3.symbol, t3.form_type, t1.period_ending, '', t1.total_assets FROM live_unified_extracts.filing_data_total_assets AS t1 INNER JOIN live_unified_extracts.sec_filing_id AS t3 ON t1.filing_id = t3.filing_id) ;

INSERT INTO live_unified_extracts.filing_data (filing_ID, cik, symbol, form_type, period_ending, period_context_ID, total_liabilities) (SELECT t2.filing_ID, t3.cik, t3.symbol, t3.form_type, t2.period_ending, '', t2.total_liabilities FROM live_unified_extracts.filing_data_total_liabilities AS t2 INNER JOIN live_unified_extracts.sec_filing_id AS t3 ON t2.filing_id = t3.filing_id ) ON CONFLICT ON constraint filing_data_pkey DO UPDATE SET total_liabilities = excluded.total_liabilities ;

INSERT INTO live_unified_extracts.filing_data (filing_ID, cik, symbol, form_type, period_ending, period_context_ID, total_revenue) (SELECT t4.filing_ID, t3.cik, t3.symbol, t3.form_type, t4.period_ending, '', t4.total_revenue FROM live_unified_extracts.filing_data_total_revenue AS t4 INNER JOIN live_unified_extracts.sec_filing_id AS t3 ON t4.filing_id = t3.filing_id ) ON CONFLICT ON constraint filing_data_pkey DO UPDATE SET total_revenue = excluded.total_revenue ;

INSERT INTO live_unified_extracts.filing_data (filing_ID, cik, symbol, form_type, period_ending, period_context_ID, operating_expenses) (SELECT t5.filing_ID, t3.cik, t3.symbol, t3.form_type, t5.period_ending, '', t5.operating_expenses FROM live_unified_extracts.filing_data_operating_expenses AS t5 INNER JOIN live_unified_extracts.sec_filing_id AS t3 ON t5.filing_id = t3.filing_id ) ON CONFLICT ON constraint filing_data_pkey DO UPDATE SET operating_expenses = excluded.operating_expenses ;

INSERT INTO live_unified_extracts.filing_data (filing_ID, cik, symbol, form_type, period_ending, period_context_ID, net_income) (SELECT t6.filing_ID, t3.cik, t3.symbol, t3.form_type, t6.period_ending, '', t6.net_income FROM live_unified_extracts.filing_data_net_income AS t6 INNER JOIN live_unified_extracts.sec_filing_id AS t3 ON t6.filing_id = t3.filing_id ) ON CONFLICT ON constraint filing_data_pkey DO UPDATE SET net_income = excluded.net_income ;

INSERT INTO live_unified_extracts.filing_data (filing_ID, cik, symbol, form_type, period_ending, period_context_ID, interest_expense) (SELECT t7.filing_ID, t3.cik, t3.symbol, t3.form_type, t7.period_ending, '', t7.interest_expense FROM live_unified_extracts.filing_data_interest_expense AS t7 INNER JOIN live_unified_extracts.sec_filing_id AS t3 ON t7.filing_id = t3.filing_id ) ON CONFLICT ON constraint filing_data_pkey DO UPDATE SET interest_expense = excluded.interest_expense ;

SELECT * FROM live_unified_extracts.filing_data LIMIT 20 ;
