#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 1000

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
typedef struct bigN {
    unsigned int *d;
    int size;  // lenths of array d
} bigN;
static void bigN_mul_add(bigN *c, int offset, uint64_t l);
static void bigN_swap(bigN *a, bigN *b);

void bigN_add(bigN *a, bigN *b, bigN *res)
{
    int newsize = MAX(a->size, b->size) + 1;  // To avoid overflow, add 1 first
    // printk("new size is %d",newsize);
    uint32_t *result = kmalloc(newsize * sizeof(uint32_t), GFP_KERNEL);

    memset(result, 0, sizeof(*result));

    uint64_t l = 0;
    for (int i = 0; i < newsize; i++) {
        uint32_t A = (a->size) > i ? a->d[i] : 0;
        uint32_t B = (b->size) > i ? b->d[i] : 0;
        l += (uint64_t) A + B;
        result[i] = l & 0xFFFFFFFF;
        l = l >> 32;
    }
    kfree(res->d);
    // Check the actual size and assign to bigN c

    if (result[newsize - 1] == 0) {
        // printf("a\n");
        newsize--;
        res->d = krealloc(result, newsize * sizeof(uint32_t), GFP_KERNEL);
        res->size = newsize;
    } else {
        // printf("b\n");
        res->d = result;
        res->size = newsize;
    }

    return;
}
bigN *bigNinit(unsigned int value)
{
    bigN *new = kmalloc(sizeof(bigN), GFP_KERNEL);
    new->d = kmalloc(sizeof(unsigned int), GFP_KERNEL);
    new->size = 1;
    new->d[0] = value;
    return new;
}
char *bigN_to_string(bigN *src)
{
    // log10(x) = log2(x) / log2(10) ~= log2(x) / 3.322
    size_t len = (8 * sizeof(int) * src->size) / 3 + 2;
    char *s = (char *) kmalloc(len, GFP_KERNEL);
    char *p = s;

    memset(s, '0', len - 1);
    s[len - 1] = '\0';
    for (int i = src->size - 1; i >= 0; i--) {
        for (unsigned int mask = 1U << 31; mask; mask >>= 1) {
            /* binary -> decimal string */
            int carry = ((mask & src->d[i]) != 0);
            for (int j = len - 2; j >= 0; j--) {
                s[j] += s[j] - '0' + carry;  // double it
                carry = (s[j] > '9');
                if (carry)
                    s[j] -= 10;
            }
        }
    }
    // skip leading zero
    while (p[0] == '0' && p[1] != '\0') {
        p++;
    }
    memmove(s, p, strlen(p) + 1);

    return s;
}
static void bigN_free(bigN *a)
{
    kfree(a->d);
    kfree(a);
    return;
}
static void bigN_swap(bigN *a, bigN *b)
{
    bigN tmp = *a;
    *a = *b;
    *b = tmp;
    return;
}
static void bigN_copy(bigN *src, bigN *dst)
{
    uint32_t *result = kmalloc((src->size) * sizeof(uint32_t), GFP_KERNEL);
    memcpy(result, src->d, src->size * sizeof(uint32_t));
    kfree(dst->d);
    dst->d = result;
}
static void bigN_resize(bigN *a)
{
    int i = a->size - 1;
    while (a->d[i] == 0) {
        i--;
    }
    a->d = krealloc(a->d, (i + 1) * sizeof(uint32_t), GFP_KERNEL);
    a->size = i + 1;
}
static void bigN_mul_add(bigN *c, int offset, uint64_t l)
{
    uint64_t carry = 0;
    for (int i = offset; i < c->size; i++) {
        carry += c->d[i] + (l & 0xFFFFFFFF);
        c->d[i] = carry;
        carry >>= 32;
        l >>= 32;
        if (!l && !carry)  // done
            return;
    }
}
static void bigN_mul(bigN *a, bigN *b, bigN *c)
{
    int newsize = (a->size) * (b->size) + 1;  // avoid overflow
    unsigned int *result = kmalloc(newsize * sizeof(unsigned int), GFP_KERNEL);
    memset(result, 0, sizeof(*result));
    kfree(c->d);
    c->d = result;
    c->size = newsize;
    uint64_t l;
    for (int i = 0; i < a->size; i++) {
        for (int j = 0; j < b->size; j++) {
            uint32_t A = (a->size) > i ? a->d[i] : 0;
            uint32_t B = (b->size) > j ? b->d[j] : 0;
            l = (uint64_t) A * (uint64_t) B;
            bigN_mul_add(c, i + j, l);
        }
    }
    bigN_resize(c);
}
static bigN *bigNfib_sequence(long long k)
{
    /* FIXME: use clz/ctz and fast algorithms to speed up */
    bigN *a = bigNinit(0);
    bigN *b = bigNinit(1);
    bigN *c = bigNinit(1);
    for (int i = 2; i <= k; i++) {
        bigN_add(a, b, c);  // a + b = c
        bigN_swap(a, b);
        bigN_swap(b, c);
    }
    // bigN_copy(b,ans);
    bigN_free(a);
    bigN_free(c);
    return b;
}
// compare two number and return a>b
static void bigN_comp(bigN *a, bigN *b)
{
    if (a->size > b->size) {
        return;
    } else if (a->size < b->size) {
        bigN_swap(a, b);
    } else {  // a->size = b->size
        int i = a->size - 1;
        // compare value
        if (a->d[i] < b->d[i]) {
            bigN_swap(a, b);
        }
    }
}
static void bigN_sub(bigN *a, bigN *b, bigN *c)
{
    bigN_comp(a, b);
    int newsize = MAX(a->size, b->size);
    unsigned int *result = kmalloc(newsize * sizeof(unsigned int), GFP_KERNEL);
    memset(result, 0, sizeof(*result));
    kfree(c->d);
    c->d = result;
    c->size = newsize;
    long long int carry = 0;
    for (int i = 0; i < c->size; i++) {
        unsigned int A = (i < a->size) ? a->d[i] : 0;
        unsigned int B = (i < b->size) ? b->d[i] : 0;
        carry = (long long int) A - B - carry;
        if (carry < 0) {
            c->d[i] = carry + (1LL << 32);
            carry = 1;
        } else {
            c->d[i] = carry;
            carry = 0;
        }
    }
    bigN_resize(c);
}
static bigN *bigN_fib_fdoubling(unsigned int n)
{
    // The position of the highest bit of n.
    // So we need to loop `h` times to get the answer.
    // Example: n = (Dec)50 = (Bin)00110010, then h = 6.
    //                               ^ 6th bit from right side
    unsigned int h = 0;
    for (unsigned int i = n; i; ++h, i >>= 1)
        ;
    bigN *f0 = bigNinit(0);
    bigN *f1 = bigNinit(1);
    bigN *a = bigNinit(1);
    bigN *b = bigNinit(1);
    bigN *f2k = bigNinit(1);
    bigN *f2k1 = bigNinit(1);

    for (unsigned int mask = 1 << (h - 1); mask; mask >>= 1) {
        /* F(2k) = F(k) * [ 2 * F(k+1) – F(k) ] */
        bigN_add(f1, f1, a);   // a = 2f1
        bigN_sub(a, f0, b);    // b = 2f1-f0
        bigN_mul(f0, b, f2k);  // f(2k)
        /* F(2k+1) = F(k)^2 + F(k+1)^2 */
        bigN_mul(f0, f0, a);   // a = f(k)^2
        bigN_mul(f1, f1, b);   // b = f(k+1)^2
        bigN_add(a, b, f2k1);  // f(2k+1)
        if (mask & n) {
            bigN_copy(f2k1, f0);
            bigN_add(f2k, f2k1, f1);
        } else {
            bigN_copy(f2k, f0);
            bigN_copy(f2k1, f1);
        }
    }
    bigN_free(f1);
    bigN_free(a);
    bigN_free(b);
    bigN_free(f2k);
    bigN_free(f2k1);
    return f0;
}
/*
static long long fib_sequence(long long k)
{
    // FIXME: use clz/ctz and fast algorithms to speed up
    long long f[k + 2];

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }

    return f[k];
}
*/
static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    ktime_t start = ktime_get();
    bigN *ans = bigNfib_sequence(*offset);
    char *str = bigN_to_string(ans);
    int retval = copy_to_user(buf, str, strlen(str));
    if (retval < 0)
        return -EFAULT;
    bigN_free(ans);
    kfree(str);
    ktime_t end = ktime_get();
    return (ssize_t) ktime_to_ns(ktime_sub(end, start));
}

/* write operation actually returns the time spent on
 * calculating the fibonacci number at given offset
 */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    ktime_t start = ktime_get();
    /*
    bigN* ans;
    switch (size)
    {
    case 0:
        ans = bigNfib_sequence(*offset);
        break;
    default:
        ans = bigN_fib_fdoubling(*offset);
        break;
    }
    */
    // char *str = bigN_to_string(ans);
    bigN *ans = bigN_fib_fdoubling(*offset);
    // char *str = bigN_to_string(ans);
    // int retval = copy_to_user(buf, str, strlen(str));
    // if (retval < 0)
    //    return -EFAULT;
    bigN_free(ans);
    // kfree(str);
    ktime_t end = ktime_get();
    return (ssize_t) ktime_to_ns(ktime_sub(end, start));
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
