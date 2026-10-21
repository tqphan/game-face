#include "head_mouse_object.h"

#include <game_face/core/expression.h>
#include <game_face/core/signals.h>

using namespace game_face;

// ------------------------------------------------------------ HeadMouseAxisObject

void HeadMouseAxisObject::load(const HeadMouseAxis& axis)
{
    source_ = QString::fromStdString(axis.source);
    hasNeutral_ = axis.neutral.has_value();
    neutral_ = axis.neutral.value_or(0);
    rangeNegative = axis.range_negative;
    rangePositive = axis.range_positive;
    gain = axis.gain;
    invert = axis.invert;
    compile();
    emit changed();
}

HeadMouseAxis HeadMouseAxisObject::toAxis() const
{
    HeadMouseAxis axis;
    axis.source = source_.toStdString();
    if (hasNeutral_)
        axis.neutral = neutral_;
    axis.range_negative = rangeNegative;
    axis.range_positive = rangePositive;
    axis.gain = gain;
    axis.invert = invert;
    return axis;
}

void HeadMouseAxisObject::setSource(const QString& source)
{
    if (source == source_)
        return;
    source_ = source;
    // A different source has a different centre.
    hasNeutral_ = false;
    compile();
    emit changed();
}

void HeadMouseAxisObject::setNeutral(double neutral)
{
    hasNeutral_ = true;
    neutral_ = neutral;
    emit changed();
}

void HeadMouseAxisObject::clearNeutral()
{
    hasNeutral_ = false;
    emit changed();
}

void HeadMouseAxisObject::compile()
{
    error_.clear();
    if (source_.trimmed().isEmpty())
        return;
    auto compiled = Expression::compile(source_.toStdString(), kSignalNames);
    if (!compiled)
        error_ = QString::fromStdString(compiled.error().message);
}

// ------------------------------------------------------------ HeadMouseGroupObject

HeadMouseGroupObject::HeadMouseGroupObject(QObject* parent)
    : QObject(parent)
    , x_(new HeadMouseAxisObject(this))
    , y_(new HeadMouseAxisObject(this))
{
    connect(x_, &HeadMouseAxisObject::changed, this, &HeadMouseGroupObject::changed);
    connect(y_, &HeadMouseAxisObject::changed, this, &HeadMouseGroupObject::changed);
}

void HeadMouseGroupObject::load(const HeadMouseGroup& group)
{
    mode_ = QString::fromUtf8(headMouseModeName(group.mode).data());
    deadzone = group.deadzone;
    smoothing = group.smoothing;
    x_->load(group.x);
    y_->load(group.y);
    emit changed();
}

HeadMouseGroup HeadMouseGroupObject::toGroup() const
{
    HeadMouseGroup group;
    group.mode = headMouseModeFromString(mode_.toStdString()).value_or(HeadMouseMode::Relative);
    group.deadzone = deadzone;
    group.smoothing = smoothing;
    group.x = x_->toAxis();
    group.y = y_->toAxis();
    return group;
}

void HeadMouseGroupObject::setMode(const QString& mode)
{
    if (mode == mode_ || !headMouseModeFromString(mode.toStdString()))
        return;
    mode_ = mode;
    emit changed();
}

QString HeadMouseGroupObject::preset() const
{
    const auto id = headMousePresetId(toGroup());
    return QString::fromUtf8(id.data(), static_cast<qsizetype>(id.size()));
}

void HeadMouseGroupObject::applyPreset(const QString& id)
{
    const HeadMousePreset* preset = findHeadMousePreset(id.toStdString());
    if (!preset)
        return;
    HeadMouseGroup group = toGroup();
    applyHeadMousePreset(group, *preset);
    load(group);
}

// ------------------------------------------------------------ HeadMouseObject

HeadMouseObject::HeadMouseObject(QObject* parent)
    : QObject(parent)
    , pointer_(new HeadMouseGroupObject(this))
    , scroll_(new HeadMouseGroupObject(this))
{
    connect(pointer_, &HeadMouseGroupObject::changed, this, &HeadMouseObject::changed);
    connect(scroll_, &HeadMouseGroupObject::changed, this, &HeadMouseObject::changed);
    load(HeadMouseSettings{});
}

void HeadMouseObject::load(const HeadMouseSettings& settings)
{
    enabled = settings.enabled;
    clickFreezeMs = settings.click_freeze_ms;
    pointer_->load(settings.pointer);
    scroll_->load(settings.scroll);
    emit changed();
}

HeadMouseSettings HeadMouseObject::toSettings() const
{
    HeadMouseSettings settings;
    settings.enabled = enabled;
    settings.click_freeze_ms = clickFreezeMs;
    settings.pointer = pointer_->toGroup();
    settings.scroll = scroll_->toGroup();
    return settings;
}
