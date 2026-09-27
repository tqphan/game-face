#include "list_models.h"

#include <game_face/core/expression.h>

#include <input_devices_simulator/script.h>

#include <algorithm>
#include <cctype>

using namespace game_face;

namespace {

bool isBlank(const std::string& s)
{
    return std::ranges::all_of(s, [](unsigned char c) { return std::isspace(c); });
}

// Empty is fine (the binding just does nothing); anything else must parse.
QString expressionError(const std::string& text)
{
    if (isBlank(text))
        return {};
    auto compiled = Expression::compile(text, kBlendshapeNames);
    return compiled ? QString() : QString::fromStdString(compiled.error().message);
}

QString commandError(const std::string& text)
{
    if (isBlank(text))
        return {};
    auto parsed = input_devices_simulator::parseCommand(text);
    return parsed ? QString() : QString::fromStdString(parsed.error().message);
}

} // namespace

// ------------------------------------------------------------ ProfileListModel

ProfileListModel::ProfileListModel(ProfileSet* set, QObject* parent)
    : QAbstractListModel(parent), set_(set)
{
}

int ProfileListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(set_->profiles.size());
}

QVariant ProfileListModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid) || role != NameRole)
        return {};
    return QString::fromStdString(set_->profiles[static_cast<std::size_t>(index.row())].name);
}

QHash<int, QByteArray> ProfileListModel::roleNames() const
{
    return {{NameRole, "name"}};
}

void ProfileListModel::reload()
{
    beginResetModel();
    endResetModel();
    emit countChanged();
}

// ------------------------------------------------------------ BindingListModel

BindingListModel::BindingListModel(ProfileSet* set, QObject* parent)
    : QAbstractListModel(parent), set_(set)
{
}

Profile* BindingListModel::profile() const
{
    if (set_->selection < 0 || set_->selection >= static_cast<int>(set_->profiles.size()))
        return nullptr;
    return &set_->profiles[static_cast<std::size_t>(set_->selection)];
}

int BindingListModel::rowCount(const QModelIndex& parent) const
{
    const Profile* p = profile();
    return parent.isValid() || !p ? 0 : static_cast<int>(p->bindings.size());
}

QHash<int, QByteArray> BindingListModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {BindingEnabledRole, "bindingEnabled"},
        {SimplifiedRole, "simplified"},
        {BlendshapeRole, "blendshape"},
        {ThresholdRole, "threshold"},
        {SimpleStartCommandRole, "simpleStartCommand"},
        {SimpleStopCommandRole, "simpleStopCommand"},
        {StartExpressionRole, "startExpression"},
        {StartCommandRole, "startCommand"},
        {StartDebounceRole, "startDebounce"},
        {StopExpressionRole, "stopExpression"},
        {StopCommandRole, "stopCommand"},
        {StopDebounceRole, "stopDebounce"},
        {StartExpressionErrorRole, "startExpressionError"},
        {StopExpressionErrorRole, "stopExpressionError"},
        {SimpleStartCommandErrorRole, "simpleStartCommandError"},
        {SimpleStopCommandErrorRole, "simpleStopCommandError"},
        {StartCommandErrorRole, "startCommandError"},
        {StopCommandErrorRole, "stopCommandError"},
        {StartActiveRole, "startActive"},
        {StopActiveRole, "stopActive"},
        {SimpleActiveRole, "simpleActive"},
        {LiveValueRole, "liveValue"},
    };
}

QVariant BindingListModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};
    const auto row = static_cast<std::size_t>(index.row());
    const Binding& b = profile()->bindings[row];
    const RowState& s = rows_[row];
    auto str = [](const std::string& v) { return QString::fromStdString(v); };

    switch (role) {
    case NameRole: return str(b.name);
    case BindingEnabledRole: return b.enabled;
    case SimplifiedRole: return b.simplified;
    case BlendshapeRole: return str(b.simple.blendshape);
    case ThresholdRole: return b.simple.threshold;
    case SimpleStartCommandRole: return str(b.simple.start_command);
    case SimpleStopCommandRole: return str(b.simple.stop_command);
    case StartExpressionRole: return str(b.start.expression);
    case StartCommandRole: return str(b.start.command);
    case StartDebounceRole: return b.start.debounce_ms;
    case StopExpressionRole: return str(b.stop.expression);
    case StopCommandRole: return str(b.stop.command);
    case StopDebounceRole: return b.stop.debounce_ms;
    case StartExpressionErrorRole: return s.start_expression_error;
    case StopExpressionErrorRole: return s.stop_expression_error;
    case SimpleStartCommandErrorRole: return s.simple_start_command_error;
    case SimpleStopCommandErrorRole: return s.simple_stop_command_error;
    case StartCommandErrorRole: return s.start_command_error;
    case StopCommandErrorRole: return s.stop_command_error;
    case StartActiveRole: return s.start_active;
    case StopActiveRole: return s.stop_active;
    case SimpleActiveRole: return s.simple_active;
    case LiveValueRole: return s.live_value;
    default: return {};
    }
}

bool BindingListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return false;
    Binding& b = profile()->bindings[static_cast<std::size_t>(index.row())];
    const std::string text = value.toString().toStdString();

    switch (role) {
    case NameRole: b.name = text; break;
    case BindingEnabledRole: b.enabled = value.toBool(); break;
    case SimplifiedRole: b.simplified = value.toBool(); break;
    case BlendshapeRole: b.simple.blendshape = text; break;
    case ThresholdRole: b.simple.threshold = std::clamp(value.toDouble(), 0.0, 100.0); break;
    case SimpleStartCommandRole: b.simple.start_command = text; break;
    case SimpleStopCommandRole: b.simple.stop_command = text; break;
    case StartExpressionRole: b.start.expression = text; break;
    case StartCommandRole: b.start.command = text; break;
    case StartDebounceRole: b.start.debounce_ms = std::max(0.0, value.toDouble()); break;
    case StopExpressionRole: b.stop.expression = text; break;
    case StopCommandRole: b.stop.command = text; break;
    case StopDebounceRole: b.stop.debounce_ms = std::max(0.0, value.toDouble()); break;
    default: return false;
    }

    validate(index.row());
    emit dataChanged(index, index);  // all roles: errors may have changed too
    emit edited();
    return true;
}

Qt::ItemFlags BindingListModel::flags(const QModelIndex& index) const
{
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

void BindingListModel::validate(int row)
{
    const Binding& b = profile()->bindings[static_cast<std::size_t>(row)];
    RowState& s = rows_[static_cast<std::size_t>(row)];
    s.start_expression_error = expressionError(b.start.expression);
    s.stop_expression_error = expressionError(b.stop.expression);
    s.simple_start_command_error = commandError(b.simple.start_command);
    s.simple_stop_command_error = commandError(b.simple.stop_command);
    s.start_command_error = commandError(b.start.command);
    s.stop_command_error = commandError(b.stop.command);
}

void BindingListModel::reload()
{
    beginResetModel();
    rows_.assign(static_cast<std::size_t>(rowCount()), RowState{});
    for (int row = 0; row < rowCount(); ++row)
        validate(row);
    endResetModel();
    emit countChanged();
}

void BindingListModel::setLiveState(std::span<const BindingStatus> status, const BlendshapeScores& scores)
{
    const Profile* p = profile();
    if (!p)
        return;
    static const QList<int> kLiveRoles = {StartActiveRole, StopActiveRole, SimpleActiveRole, LiveValueRole};

    for (std::size_t row = 0; row < rows_.size() && row < status.size(); ++row) {
        RowState& s = rows_[row];
        const auto shape = blendshapeIndex(p->bindings[row].simple.blendshape);
        const double live = shape ? scores[*shape] : 0.0;
        if (s.start_active == status[row].start_active && s.stop_active == status[row].stop_active
            && s.simple_active == status[row].simple_active && s.live_value == live)
            continue;
        s.start_active = status[row].start_active;
        s.stop_active = status[row].stop_active;
        s.simple_active = status[row].simple_active;
        s.live_value = live;
        const QModelIndex i = index(static_cast<int>(row));
        emit dataChanged(i, i, kLiveRoles);
    }
}

void BindingListModel::clearLiveState()
{
    std::vector<BindingStatus> idle(rows_.size());
    setLiveState(idle, BlendshapeScores{});
}

// ------------------------------------------------------------ BlendshapeModel

int BlendshapeModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(kBlendshapeNames.size());
}

QVariant BlendshapeModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};
    const auto row = static_cast<std::size_t>(index.row());
    if (role == NameRole)
        return QString::fromUtf8(kBlendshapeNames[row].data(), static_cast<qsizetype>(kBlendshapeNames[row].size()));
    if (role == ValueRole)
        return scores_[row];
    return {};
}

QHash<int, QByteArray> BlendshapeModel::roleNames() const
{
    return {{NameRole, "name"}, {ValueRole, "value"}};
}

void BlendshapeModel::setScores(const BlendshapeScores& scores)
{
    if (scores == scores_)
        return;
    scores_ = scores;
    emit dataChanged(index(0), index(rowCount() - 1), {ValueRole});
}
