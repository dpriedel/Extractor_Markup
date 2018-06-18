-- automate our connection

-- dbext:profile=edgar_extracts_DBI

select count(*) from live_extracts.edgar_filing_data where user_label = 'Missing Value' ;
select count(distinct(filing_id)) from live_extracts.edgar_filing_data where user_label = 'Missing Value' ;


select * from live_extracts.edgar_filing_data where user_label = 'Missing Value' limit 20;

select t2.filing_id, t2.cik, t2.company_name, t2.file_name from  (select distinct(filing_id) from live_extracts.edgar_filing_data where user_label = 'Missing Value') as t1 inner join live_extracts.edgar_filing_id as t2 on t1.filing_id = t2.filing_id limit 5;

select filing_id, count(*) as ctr from live_extracts.edgar_filing_data where user_label = 'Missing Value' group by filing_id order by ctr desc limit 20;


select t2.filing_id, t2.cik, t2.company_name, t2.file_name t1.ctr from (select filing_id, count(*) as ctr from live_extracts.edgar_filing_data where user_label = 'Missing Value' group by filing_id order by ctr desc limit 20) as t1 inner join live_extracts.edgar_filing_id as t2 on t1.filing_id = t2.filing_id;
