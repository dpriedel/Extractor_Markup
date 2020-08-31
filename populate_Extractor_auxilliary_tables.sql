
insert into unified_extracts.filing_data_total_revenue (select t1.filing_id, t1.cik, t1.form_type, t1.period_ending,  MAX(abs(t2.value::REAL)) AS total_revenue from unified_extracts.sec_filing_id as t1 inner join unified_extracts.sec_stmt_of_ops_data as t2
 on t1.filing_id = t2.filing_id  where t2.tsv_index_col @@ to_tsquery('total <-> revenue') group by t1.cik, t1.period_ending, t1.form_type,  t1.filing_id order by t1.cik, t1.period_ending asc );

select * from unified_extracts.filing_data_total_revenue ;

delete from unified_extracts.filing_data_total_revenue ;
