Extractor_Markup
================

Program which loads the content of SEC EDGAR data files which have been previously
downloaded by the Collector program (or other program) into a Postgresql database.

May 12, 2021. Version 5.0 update (why diddle around with version 1?):

I should have done a release on this a long time ago, but better late than never.

The program reads selected files from a directory hierarchy (as created by Collector) and extracts the contents
for financial statements and loads them into a set of database tables.  Most of the data is free-form as there 
is not set format for field labels. As such, the fields in the database are indexed for full-text search.

Currently, the program can extract data from the HTML content (all files have this), the XBRL content (all files 
since 200x have that) or, from XLSX files which are generally included with the XBRL.  Unless specified otherwise, 
the order of preference is: XLSX, XBRL, HTML.

Older files, prior to 200x have only the HTML.  Some of these older files don't even have real HTML content. Consequently, 
extracting data from the HTML is problematic.  A 'best effort' is made.

Running ExtractorApp with no arguments will print a help message showing the options.

The program can process files concurrently.



