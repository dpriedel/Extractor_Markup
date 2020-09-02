
delete from unified_extracts.filing_data_total_revenue ;

insert into unified_extracts.filing_data_total_revenue (select t1.filing_id, t1.cik, t1.form_type, t1.period_ending,  MAX(abs(t2.value::REAL)) AS total_revenue from unified_extracts.sec_filing_id as t1 inner join unified_extracts.sec_stmt_of_ops_data as t2
 on t1.filing_id = t2.filing_id  where t2.tsv_index_col @@ to_tsquery('total <-> revenue') group by t1.cik, t1.period_ending, t1.form_type,  t1.filing_id order by t1.cik, t1.period_ending asc );

select * from unified_extracts.filing_data_total_revenue LIMIT 20 ;

delete from unified_extracts.filing_data_net_income ;

insert into unified_extracts.filing_data_net_income (select t1.filing_id, t1.cik, t1.form_type, t1.period_ending,  MAX(abs(t2.value::REAL)) AS total_revenue from unified_extracts.sec_filing_id as t1 inner join unified_extracts.sec_stmt_of_ops_data as t2
 on t1.filing_id = t2.filing_id  where t2.tsv_index_col @@ to_tsquery('net <-> income') group by t1.cik, t1.period_ending, t1.form_type,  t1.filing_id order by t1.cik, t1.period_ending asc );

select * from unified_extracts.filing_data_net_income LIMIT 20 ;

delete from unified_extracts.filing_data_interest_expense ;

insert into unified_extracts.filing_data_interest_expense (select t1.filing_id, t1.cik, t1.form_type, t1.period_ending,  MAX(abs(t2.value::REAL)) AS total_revenue from unified_extracts.sec_filing_id as t1 inner join unified_extracts.sec_stmt_of_ops_data as t2
 on t1.filing_id = t2.filing_id  where t2.tsv_index_col @@ to_tsquery('interest <-> expense') group by t1.cik, t1.period_ending, t1.form_type,  t1.filing_id order by t1.cik, t1.period_ending asc );

select * from unified_extracts.filing_data_interest_expense LIMIT 20 ;

delete from unified_extracts.filing_data_operating_expenses ;

insert into unified_extracts.filing_data_operating_expenses (select t1.filing_id, t1.cik, t1.form_type, t1.period_ending,  MAX(abs(t2.value::REAL)) AS total_revenue from unified_extracts.sec_filing_id as t1 inner join unified_extracts.sec_stmt_of_ops_data as t2
 on t1.filing_id = t2.filing_id  where t2.tsv_index_col @@ to_tsquery('operating <-> expense') group by t1.cik, t1.period_ending, t1.form_type,  t1.filing_id order by t1.cik, t1.period_ending asc );

select * from unified_extracts.filing_data_operating_expenses LIMIT 20 ;

delete from unified_extracts.filing_data_total_assets ;

insert into unified_extracts.filing_data_total_assets (select t1.filing_id, t1.cik, t1.form_type, t1.period_ending,  MAX(abs(t2.value::REAL)) AS total_revenue from unified_extracts.sec_filing_id as t1 inner join unified_extracts.sec_bal_sheet_data as t2
 on t1.filing_id = t2.filing_id  where t2.tsv_index_col @@ to_tsquery('total <-> assets') group by t1.cik, t1.period_ending, t1.form_type,  t1.filing_id order by t1.cik, t1.period_ending asc );

select * from unified_extracts.filing_data_total_assets LIMIT 20 ;

delete from unified_extracts.filing_data_total_liabilities ;

insert into unified_extracts.filing_data_total_liabilities (select t1.filing_id, t1.cik, t1.form_type, t1.period_ending,  MAX(abs(t2.value::REAL)) AS total_revenue from unified_extracts.sec_filing_id as t1 inner join unified_extracts.sec_bal_sheet_data as t2
 on t1.filing_id = t2.filing_id  where t2.tsv_index_col @@ to_tsquery('total <-> liabilities') group by t1.cik, t1.period_ending, t1.form_type,  t1.filing_id order by t1.cik, t1.period_ending asc );

select * from unified_extracts.filing_data_total_liabilities LIMIT 20 ;

