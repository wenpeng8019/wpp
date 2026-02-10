## BEGIN / BEGIN TRANSACTION
开始事务
```sql
BEGIN;
BEGIN TRANSACTION;
BEGIN DEFERRED TRANSACTION;
BEGIN IMMEDIATE TRANSACTION;
BEGIN EXCLUSIVE TRANSACTION;
```
**HTTP 映射建议**: `POST` + Header: `X-SQTP-Command: BEGIN-TRANSACTION`

## COMMIT / END
提交事务
```sql
COMMIT;
COMMIT TRANSACTION;
END;
END TRANSACTION;
```
**HTTP 映射建议**: `POST` + Header: `X-SQTP-Command: COMMIT`



## ROLLBACK

回滚事务
```sql
ROLLBACK;
ROLLBACK TRANSACTION;
ROLLBACK TO savepoint_name;
```
**HTTP 映射建议**: `POST` + Header: `X-SQTP-Command: ROLLBACK`



## SAVEPOINT

创建保存点
```sql
SAVEPOINT savepoint_name;
```
**HTTP 映射建议**: `POST` + Header: `X-SQTP-Command: SAVEPOINT`

#### RELEASE
释放保存点
```sql
RELEASE savepoint_name;
RELEASE SAVEPOINT savepoint_name;
```
**HTTP 映射建议**: `POST` + Header: `X-SQTP-Command: RELEASE`

---

