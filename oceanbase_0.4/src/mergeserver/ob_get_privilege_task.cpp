/* (C) 2010-2012 Alibaba Group Holding Limited.
 *
 * This program is free software: you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 *
 * Version: 0.1
 *
 * Authors:
 *    Wu Di <lide.wd@taobao.com>
 */

#include "ob_get_privilege_task.h"
#include "sql/ob_result_set.h"
#include "common/ob_privilege.h"
using namespace oceanbase;
using namespace oceanbase::mergeserver;
using namespace oceanbase::sql;
using namespace oceanbase::common;

const ObString & ObGetPrivilegeTask::get_users = ObString::make_string("SELECT /*+read_consistency(WEAK) */ u.user_name,u.user_id,u.pass_word,u.info,u.priv_all,u.priv_alter,u.priv_create,u.priv_create_user,u.priv_delete,u.priv_drop,u.priv_grant_option,u.priv_insert,u.priv_update,u.priv_select,u.priv_replace, u.is_locked  FROM __all_user u");
const ObString & ObGetPrivilegeTask::get_table_privileges = ObString::make_string("SELECT /*+read_consistency(WEAK) */t.user_id,t.table_id,t.priv_all,t.priv_alter,t.priv_create,t.priv_create_user,t.priv_delete,t.priv_drop,t.priv_grant_option,t.priv_insert,t.priv_update,t.priv_select, priv_replace FROM __all_table_privilege t");
ObGetPrivilegeTask::ObGetPrivilegeTask()
  : privilege_mgr_(NULL), sql_proxy_(NULL), privilege_version_(0)
{
}
ObGetPrivilegeTask::~ObGetPrivilegeTask()
{
}
void ObGetPrivilegeTask::init(ObPrivilegeManager *privilege_mgr, ObMsSQLProxy *sql_proxy, const int64_t privilege_version)
{
  privilege_mgr_ = privilege_mgr;
  sql_proxy_ = sql_proxy;
  privilege_version_ = privilege_version;
}

void ObGetPrivilegeTask::runTimerTask()
{
  int ret = OB_SUCCESS;
  //ObString get_privilege_version = ObString::make_string("SELECT value1 FROM __all_sys_stat where name='ob_current_privilege_version'");

  TBSYS_LOG(INFO, "GET PRIVILEGE NOW");
  ObPrivilege *p_privilege = (reinterpret_cast<ObPrivilege*>(ob_malloc(sizeof(ObPrivilege), ObModIds::OB_SQL_PRIVILEGE)));
  if (NULL == p_privilege)
  {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    TBSYS_LOG(ERROR, "ob malloc failed,ret=%d", ret);
  }
  else
  {
    p_privilege = new (p_privilege) ObPrivilege();
    if (OB_SUCCESS != (ret = p_privilege->init()))
    {
      TBSYS_LOG(WARN, "init privilege failed, ret=%d", ret);
    }
    else if (OB_SUCCESS != (ret = load_user_table(p_privilege)))
    {
      TBSYS_LOG(WARN, "load user table failed, ret=%d", ret);
    }
    else if (OB_SUCCESS != (ret = load_privilege_table(p_privilege)))
    {
      TBSYS_LOG(WARN, "load privilege table failed, ret=%d", ret);
    }
    else
    {
      p_privilege->set_version(privilege_version_);
      if (OB_SUCCESS != (ret = privilege_mgr_->renew_privilege(*p_privilege)))
      {
        TBSYS_LOG(WARN, "renew privilege failed, ret=%d", ret);
      }
    }
    if (OB_UNLIKELY(OB_SUCCESS != ret))
    {
      p_privilege->~ObPrivilege();
      ob_free(p_privilege);
      p_privilege = NULL;
    }
  }
}

int ObGetPrivilegeTask::load_user_table(ObPrivilege *p_privilege)
{
  int ret = OB_SUCCESS;
  ObSQLResultSet sql_rs;
  ObResultSet &rs = sql_rs.get_result_set();
  ObSQLSessionInfo session;
  ObSqlContext context;
  int64_t schema_version = 0;
  if (OB_SUCCESS != (ret = sql_proxy_->init_sql_env(context, schema_version, sql_rs, session)))
  {
    TBSYS_LOG(WARN, "init sql env error, err=%d", ret);
  }
  else
  {
    if (OB_SUCCESS != (ret = sql_proxy_->execute(get_users, sql_rs, context, schema_version)))
    {
      TBSYS_LOG(WARN, "execute privilge sql failed,sql=%.*s, ret=%d", get_users.length(), get_users.ptr(), ret);
    }
    else if (OB_SUCCESS != (ret = rs.open()))
    {
      TBSYS_LOG(ERROR, "open rs failed, ret=%d", ret);
    }
    else
    {
      const ObRow *row = NULL;
      while (OB_SUCCESS == ret)
      {
        if (OB_SUCCESS != (ret = rs.get_next_row(row)))
        {
          if (OB_ITER_END == ret)
          {
            ret = OB_SUCCESS;
          }
          else
          {
            TBSYS_LOG(ERROR, "failed to get next row, ret=%d", ret);
          }
          break;
        }
        else if (OB_SUCCESS != (ret = p_privilege->add_users_entry(*row)))
        {
          TBSYS_LOG(WARN, "add user privilege to ObPrivilege failed, ret=%d", ret);
          break;
        }
      }
    }
    sql_proxy_->cleanup_sql_env(context, sql_rs);
  }
  return ret;
}

int ObGetPrivilegeTask::load_privilege_table(ObPrivilege *p_privilege)
{
  int ret = OB_SUCCESS;
  ObSQLResultSet sql_rs;
  ObResultSet &rs = sql_rs.get_result_set();
  ObSQLSessionInfo session;
  ObSqlContext context;
  int64_t schema_version = 0;
  if (OB_SUCCESS != (ret = sql_proxy_->init_sql_env(context, schema_version, sql_rs, session)))
  {
    TBSYS_LOG(WARN, "init sql env error, err=%d", ret);
  }
  else
  {
    if (OB_SUCCESS != (ret = sql_proxy_->execute(get_table_privileges, sql_rs, context, schema_version)))
    {
      TBSYS_LOG(WARN, "execute privilge sql failed,sql=%.*s, ret=%d", get_table_privileges.length(), get_table_privileges.ptr(), ret);
    }
    else if (OB_SUCCESS != (ret = rs.open()))
    {
      TBSYS_LOG(ERROR, "open rs failed, ret=%d", ret);
    }
    else
    {
      const ObRow *row = NULL;
      while (OB_SUCCESS == ret)
      {
        if (OB_SUCCESS != (ret = rs.get_next_row(row)))
        {
          if (OB_ITER_END == ret)
          {
            ret = OB_SUCCESS;
          }
          else
          {
            TBSYS_LOG(ERROR, "failed to get next row, ret=%d", ret);
          }
          break;
        }
        else if (OB_SUCCESS != (ret = p_privilege->add_table_privileges_entry(*row)))
        {
          TBSYS_LOG(WARN, "add table privilege to ObPrivilege failed, ret=%d", ret);
        }
      }
    }
    sql_proxy_->cleanup_sql_env(context, sql_rs);
  }
  return ret;
}
