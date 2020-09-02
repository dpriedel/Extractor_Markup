delete from unified_extracts.filing_data ;

insert into unified_extracts.filing_data (select t1.filing_ID, t3.cik, t3.symbol, t3.form_type, t1.period_ending, '', t1.total_assets, t2.total_liabilities, t4.total_revenue, t5.operating_expenses, t6.net_income, t7.interest_expense from unified_extracts.filing_data_total_assets as t1 inner join unified_extracts.filing_data_total_liabilities as t2 on t2.filing_id = t1.filing_id inner join unified_extracts.sec_filing_id as t3 on t1.filing_id = t3.filing_id inner join unified_extracts.filing_data_total_revenue as t4 on t4.filing_ID = t1.filing_ID inner join unified_extracts.filing_data_operating_expenses as t5 on t5.filing_ID = t1.filing_ID inner join unified_extracts.filing_data_net_income as t6 on t6.filing_ID = t1.filing_ID inner join unified_extracts.filing_data_interest_expense as t7 on t7.filing_ID = t1.filing_ID order by t3.cik, t1.period_ending) ;

select * from unified_extracts.filing_data limit 50 ;
