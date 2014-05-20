#include <canopy.h>
#include <sys/reboot.h>

#define CANOPY_CLOUD_HOST "sandbox.canopy.link"
#define CANOPY_CLOUD_PORT 8080
#define CANOPY_CLOUD_USERNAME "myusername"
#define CANOPY_CLOUD_PASSWORD "mypassword"

static float get_cpu_usage()
{
    FILE *fp;
    float val;
    fp = fopen("/proc/loadavg", "r");
    fscanf(fp, "%f", &val);
    fclose(fp);
    return val;
}

static bool handle_canopy_event(CanopyContext ctx, CanopyEventDetails event)
{
    switch (canopy_get_event_type(event))
    {
        case CANOPY_EVENT_CONNECTION_ESTABLISHED:
        {
            printf("Connection established!");
            break;
        }
        case CANOPY_EVENT_CONNECTION_LOST:
        {
            printf("Connection lost!");
            break;
        }
        case CANOPY_EVENT_REPORT_REQUESTED:
        {
            CanopyReport report;
            float cpu = get_cpu_usage();

            report = canopy_begin_report(ctx);
            canopy_report_float32(report, "cpu", cpu);
            canopy_send_report(report);
            break;
        }
        case CANOPY_EVENT_CONTROL_TRIGGER:
        {
            if (canopy_event_control_name_matches(event, "reboot"))
            {
                reboot(RB_AUTOBOOT);
                break;
            }
        }
        default:
        {
            break;
        }
    }
    return true;
}

int main(int argc, const char *argv[])
{
    CanopyContext ctx;
    if (argc < 2)
    {
        printf("USAGE: ./my_canopy_client <DEVICE_ID>\n");
        return -1;
    }
    ctx = canopy_init();
    canopy_load_device_description(ctx, "tutorial.sddl", "canopy.tutorial.sample_device_1");

    canopy_register_event_callback(ctx, handle_canopy_event, NULL);

    canopy_set_cloud_host(ctx, CANOPY_CLOUD_HOST);
    canopy_set_cloud_port(ctx, CANOPY_CLOUD_PORT);
    canopy_set_cloud_username(ctx, CANOPY_CLOUD_USERNAME);
    canopy_set_cloud_password(ctx, CANOPY_CLOUD_PASSWORD);
    canopy_set_device_id(ctx, argv[1]);
    canopy_set_auto_reconnect(ctx, true);
    canopy_connect(ctx);

    canopy_event_loop(ctx);
    canopy_shutdown(ctx);
    return 0;
}
