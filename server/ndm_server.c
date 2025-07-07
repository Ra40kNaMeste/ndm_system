#include "ndm_server.h"
#include <linux/kernel.h>
#include <linux/slab.h> 
#include <linux/string.h>  


/* attribute policy */
static struct nla_policy ndm_calculate_policy[NDS_SERVER_A_MAX + 1] = {
    [DATA] = { .type = NLA_NUL_STRING },
};

static struct genl_family ndm_family;


//Блок калькулятора
int perform_action(const char *action, int arg1, int arg2) {
    if (strcmp(action, "add") == 0) {
        return arg1 + arg2;
    } else if (strcmp(action, "sub") == 0) {
        int res=  arg1-arg2;
        return res;
    } else if (strcmp(action, "mul") == 0) {
        return arg1 * arg2;
    } else {
        pr_err("NDM_SERVER: Unknown action");
        return 0;
    }
}

//Функция отправки ответа от сервера
static int reply(const char* json, struct genl_info *info){
    struct sk_buff *reply_skb;
    void *reply_hdr;
    int ret;

    // Создаем буфер для ответа
    reply_skb = genlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
    if (!reply_skb) {
        pr_err("NDM_SERVER: Error allocating memory for response\n");
        return -ENOMEM;
    }

    // Формируем заголовок ответа
    reply_hdr = genlmsg_put(reply_skb, 0, info->snd_seq, &ndm_family, 0, info->genlhdr->cmd);
    if (!reply_hdr) {
        nlmsg_free(reply_skb);
        pr_err("NDM_SERVER: Error creating response header\n");
        return -ENOMEM;
    }

    // Добавляем атрибуты
    ret = nla_put_string(reply_skb, DATA, json);
    if (ret) {
        genlmsg_cancel(reply_skb, reply_hdr);
        nlmsg_free(reply_skb);
        pr_err("NDM_SERVER: Error added attributes\n");
        return ret;
    }

    // Завершаем формирование сообщения
    genlmsg_end(reply_skb, reply_hdr);

    // Отправляем ответ клиенту (Unicast)
    ret = genlmsg_unicast(genl_info_net(info), reply_skb, info->snd_portid);
    if (ret) {
        pr_err("NDL_SERVER: Error sending response: %d\n", ret);
    }
    return ret;
}

// Функция отправки сообщения об ошибке
static int reply_error(const char* message, struct genl_info *info){
     char buf[128];

    // snprintf безопасно соберёт строку
    int len = snprintf(buf, sizeof(buf), "{\r\n\"error\":\"%s\"\r\n}", message);
    if (len >= sizeof(buf)) {
        pr_err("NDM_SERVER: Buffer overflow in reply_error\n");
        return -EINVAL;
    }

    return reply(buf, info);
}


// Функция отправки результата
static int reply_result(int result, struct genl_info *info){
    char buf[64];
    int len;
    
    len = snprintf(buf, sizeof(buf), "{\r\n\"result\":%d\r\n}", result);
    if (len >= sizeof(buf)) {
        pr_err("NDM_SERVER: Buffer overflow in reply_result\n");
        return -EINVAL;
    }
    
    return reply(buf, info);
}


int parse_json_message(const char *input, char *action, int *arg1, int *arg2)
{

    char buf[256];
    char *src = (char *)input;
    char *dst = buf;

    while (*src && (dst - buf) < sizeof(buf) - 1) {
        if (*src != '\n' && *src != '\r' && *src != ' ')
            *dst++ = *src;
        src++;
    }
    *dst = '\0';
    pr_info("%s", buf);
    if (sscanf(buf,
               "{\"action\":\"%3[^\"]\",\"argument_1\":%d,\"argument_2\":%d}",
               action, arg1, arg2) == 3) {
        pr_info("Ok");
        return 0;
    }

    pr_err("NDM_SERVER: JSON parse error: input='%s'\n", buf);
    return -EINVAL;
}

/* handler */
 static int calculate_handler(struct sk_buff *skb, struct genl_info *info)
{

    pr_info("NDM_SERVER:Ping received from userspace!\n");
    if(!info || !info->attrs){
        return reply_error("NDM_SERVER: Missing required attribute 'Info'", info);
    }

    if (info->attrs[DATA]) {
        char *msg = nla_data(info->attrs[DATA]);
        pr_info("NDM_SERVER: Message: %s\n", msg);

        // Ручной парсинг json
        char action[4]; //Т.к. add sub и mul имеют 3 символа + \0
        int arg1, arg2;
        if (parse_json_message(msg, action, &arg1, &arg2) != 0) {
            return -EINVAL;
        }
        action[3] = '\0';

        int res = perform_action(action, arg1, arg2);
        return reply_result(res, info);
    }
    return reply_error("NDM_SERVER: Missing required attribute 'Data'", info);
}

/* operation definition */
static struct genl_ops ndm_gnl_ops[] = {
    {
        .cmd = CALCULATE_FROM_JSON,
        .flags = 0,
        .policy = ndm_calculate_policy,
        .doit = calculate_handler,
        .dumpit = NULL,
    }
};

/* family definition */
static struct genl_family ndm_family = {
    .hdrsize = 0,
    .name = FAMILY_NAME,
    .version = FAMILY_VERSION,
    .ops = ndm_gnl_ops,
    .n_ops = ARRAY_SIZE(ndm_gnl_ops),
    .maxattr = NDS_SERVER_A_MAX,
};


static ssize_t ping_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *buf, size_t cnt)
{
	int max = cnt;
	//calculate_handler(buf, max);

	return max;
}

static struct kobject *kobj;
static struct kobj_attribute ping_attr = __ATTR_WO(ping);

static int __init init_ndm_server(void)
{
    pr_info("NDM_SERVER:init start\n");
	kobj = kobject_create_and_add("ndm_server", kobj);
	if (unlikely(!kobj)) {
        pr_err("NDM_SERVER:unable to create kobject\n");
		return -ENOMEM;
	}

	int ret = genl_register_family(&ndm_family);
	if (unlikely(ret)) {
        pr_crit("NDM_SERVER:failed to register generic netlink family\n");
		kobject_put(kobj);
	}
    pr_info("NDM_SERVER:init end\n");
	return ret;
}

static void __exit exit_ndm_server(void)
{
	if (unlikely(genl_unregister_family(&ndm_family))) {
		pr_err("NDM_SERVER:failed to unregister generic netlink family\n");
	}

	kobject_put(kobj);

	pr_info("NDM_SERVER:exit\n");
}



module_init(init_ndm_server);
module_exit(exit_ndm_server);

MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("Ra40kNaMeste");